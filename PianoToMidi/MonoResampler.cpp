#include "stdafx.h"
#include "FFmpegError.h"
#include "MonoResampler.h"

using namespace std;

MonoResampler::~MonoResampler()
{
	if (srcData_) av_freep(&srcData_[0]);
	av_freep(&srcData_);

	if (dstData_) av_freep(&dstData_[0]);
	av_freep(&dstData_);

	swr_free(&context_);
}

pair<const uint8_t*, size_t> MonoResampler::Resample(uint8_t* srcData, const size_t srcBytes,
	int srcChannels, const int srcRate, const int dstRate,
	const AVSampleFormat srcSampleFmt, const AVSampleFormat dstSampleFmt)
{
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