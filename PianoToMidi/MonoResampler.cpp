#include "stdafx.h"
#include "FFmpegError.h"
#include "MonoResampler.h"

using namespace std;

MonoResampler::~MonoResampler()
{
	assert(srcData_ and dstData_ and "Did you call Resample? If not, why did you instantiate this class?");

	if (srcData_) av_freep(&srcData_[0]);
	av_freep(&srcData_);

	if (dstData_) av_freep(&dstData_[0]);
	av_freep(&dstData_);

	swr_free(&context_);
}

pair<uint8_t*, size_t> MonoResampler::FFmpegResample(uint8_t* srcData, const size_t srcBytes, const int srcChannels,
	const int srcRate, const int dstRate, const AVSampleFormat srcSampleFmt, const AVSampleFormat dstSampleFmt)
{
	assert(not srcData_ and not dstData_ and
		"Resample must be called just once, then destructor should deallocate all internal data");

	context_ = swr_alloc_set_opts(context_, av_get_default_channel_layout(1),
		dstSampleFmt, static_cast<int>(dstRate), av_get_default_channel_layout(srcChannels),
		srcSampleFmt, static_cast<int>(srcRate), 0, nullptr);
	if (swr_init(context_) < 0) throw FFmpegError("Could not initialize the resampling context");

	const auto bytesPerSample(av_get_bytes_per_sample(srcSampleFmt));
	if (bytesPerSample == 0) throw FFmpegError("Could not determine number of bytes per sample");
	const auto srcNsamples(static_cast<int>(srcBytes / bytesPerSample));

	// Allocated destination buffer will strangely intersect with source data (from our vector.data())
	// So, first reallocate source again, further away, so it won't get touched:
	int srcLineSize;
	if (av_samples_alloc_array_and_samples(&srcData_, &srcLineSize, srcChannels, srcNsamples,
		srcSampleFmt, 0) < 0) throw FFmpegError("Could not allocate source samples");
	CopyMemory(*srcData_, srcData, srcBytes);

	auto dstNsamplesMax(av_rescale_rnd(srcNsamples, dstRate, srcRate, AV_ROUND_UP)),
		dstNsamples(dstNsamplesMax);
	int dstLineSize;
	// Directly write rawaudio buffer, no alignment:
	if (av_samples_alloc_array_and_samples(&dstData_, &dstLineSize, 1, static_cast<int>(dstNsamples),
		dstSampleFmt, 0) < 0) throw FFmpegError("Could not allocate destination samples");

	dstNsamples = av_rescale_rnd(swr_get_delay(context_, srcRate) + srcNsamples, dstRate, srcRate, AV_ROUND_UP);
	if (dstNsamples > dstNsamplesMax)
	{
		av_freep(&dstData_[0]);
		if (av_samples_alloc(dstData_, &dstLineSize, 1, static_cast<int>(dstNsamples), dstSampleFmt, 1) < 0)
			throw FFmpegError("Could not allocate destination samples");
	}

	const auto dstNsamplesFinal(swr_convert(context_, dstData_, static_cast<int>(dstNsamples),
		const_cast<const uint8_t**>(srcData_), srcNsamples));
	if (dstNsamplesFinal < 0) throw FFmpegError("Could not resample the audio");
	const auto dstBytes(av_samples_get_buffer_size(&dstLineSize, 1, dstNsamplesFinal, dstSampleFmt, 1));
	if (dstBytes < 0) throw FFmpegError("Could not get sample buffer size");

	return make_pair(*dstData_, static_cast<size_t>(dstBytes));
}

vector<float> MonoResampler::ResampyResample(const vector<float>& srcData, const int srcRate, const int dstRate) const
{
	using placeholders::_1;
	using boost::filesystem::exists;
	using namespace boost::archive;

	assert(srcData_ and dstData_ and "FFmpegResample must have been called before ResampyResample");
	assert(srcRate > 0 and dstRate > 0 and "Sample rates must be positive");
	if (srcRate == dstRate) return srcData;

	const auto ratio(static_cast<double>(dstRate) / srcRate);
	const auto shape(static_cast<size_t>(srcData.size() * ratio));
	if (shape < 1)
	{
		ostringstream os;
		os << "Input signal length = " << srcData.size() << " is too small to resample from " << srcRate << " --> " << dstRate;
		throw FFmpegError(os.str().c_str());
	}

	static array<double, 32'769> interpWin;
	if (all_of(interpWin.cbegin(), interpWin.cend(), bind(equal_to<double>(), 0, _1)))
	{
		const auto interpWinFile("Resample Interp Win.dat");
		if (not exists(interpWinFile)) throw FFmpegError((string("Could not find data file: \"") + interpWinFile + "\". Please, put it back").c_str());
		ifstream ifs(interpWinFile, ifstream::binary);
		try { binary_iarchive(ifs) >> interpWin; }
		catch (const archive_exception& e) { throw FFmpegError(e.what()); }
	}

	auto interpWinScaled(make_unique<array<double, 32'769>>()), interp_delta(make_unique<array<double, 32'769>>());
	if (ratio < 1) transform(interpWin.cbegin(), interpWin.cend(), interpWinScaled->begin(), bind(multiplies<double>(), ratio, _1));
	adjacent_difference(next(interpWinScaled->cbegin()), interpWinScaled->cend(), interp_delta->begin());
	interp_delta->front() = interpWinScaled->at(1) - interpWinScaled->front();
	interp_delta->back() = 0;

	vector<double> result(shape);
	const auto scale(min(1., ratio));
	const auto num_table(512), // precision (number of samples between zero-crossings of the fitler)
		index_step(static_cast<int>(scale * num_table));
	double time_register(0);
	for (size_t t(0); t < result.size(); ++t)
	{
		const auto n(static_cast<size_t>(time_register)); // Grab the top bits as an index to the input buffer

		auto frac(scale * (time_register - n)), // Grab the fractional component of the time index
			index_frac(frac * num_table);
		auto offset(static_cast<int>(index_frac));
		for (size_t i(0); i < min(n + 1, (interpWinScaled->size() - offset) / index_step); ++i) // Left wing of the filter response
			result.at(t) += (interpWinScaled->at(offset + i * index_step) + (index_frac - offset) /*eta = interpolation factor*/ * interp_delta->at(offset + i * index_step)) // weight
				* srcData.at(n - i);

		index_frac = (scale - frac) /*Invert P*/ * num_table;
		offset = static_cast<int>(index_frac);
		for (size_t k(0); k < min(srcData.size() - n - 1, (interpWinScaled->size() - offset) / index_step); ++k) // Right wing of the filter response 
			result.at(t) += (interpWinScaled->at(offset + k * index_step) + (index_frac - offset) /*eta = interpolation factor*/ * interp_delta->at(offset + k * index_step)) // weight
				* srcData.at(n + k + 1);
			
		time_register += 1 / ratio;
	}
	result.resize(min(result.size(), static_cast<size_t>(ceil(srcData.size() * ratio))));

	return move(vector<float>(result.cbegin(), result.cend()));
}