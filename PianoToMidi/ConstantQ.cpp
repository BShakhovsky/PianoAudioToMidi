#include "stdafx.h"

#include "AlignedVector.h"
#include "ConstantQ.h"
#include "AudioLoader.h"

#include "CqtBasis.h"
#include "ShortTimeFourier.h"
#include "CqtError.h"

#include "IntelCheckStatus.h"

using namespace std;

int Num2factors(int x)
{
	// How many times x can be evenly divided by 2
	if (x <= 0) return 0;
	int result(0);
	for (; x % 2 == 0; x /= 2) ++result;
	return result;
}

ConstantQ::ConstantQ(const shared_ptr<class AudioLoader>& audio, const size_t nBins,
	const int octave, const float fMin, const int hopLen, const float filtScale, const NORM_TYPE norm,
	const float sparsity, const CQT_WINDOW window, const bool toScale, const bool isPadReflect)
	: nBins_(nBins), qBasis_(make_unique<CqtBasis>(octave, filtScale, norm, window)),
	stft_(nullptr), audio_(audio), hopLen_(hopLen)
{
	/* The recursive sub-sampling method described by
	Schoerkhuber, Christian, and Anssi Klapuri
	"Constant-Q transform toolbox for music processing."
	7th Sound and Music Computing Conference, Barcelona, Spain. 2010 */

	assert(audio_->GetBytesPerSample() == sizeof(float) and
		"Raw audio data is assumed to be in float-format before calculating CQT-spectrogram");

	qBasis_->CalcFrequencies(audio_->GetSampleRate(), fMin, nBins);
	auto fMinOctave(*(qBasis_->GetFrequencies().cend() - octave)), // First, frequencies of the top octave
		fMaxOctave(qBasis_->GetFrequencies().back());
	assert(fMinOctave == *min_element(qBasis_->GetFrequencies().cend() - octave,
		qBasis_->GetFrequencies().cend()) and fMaxOctave == *max_element(
			qBasis_->GetFrequencies().cend() - octave, qBasis_->GetFrequencies().cend())
		and "CQT-frequencies are wrong");

	auto nOctaves(static_cast<int>(ceil(static_cast<float>(nBins) / octave)));
	auto filterCutoff(fMaxOctave * (1 + .5 *WIN_BAND_WIDTH[
		static_cast<int>(window)] / qBasis_->GetQfactor())); // Required resampling quality:
	const bool isKaiserFast(filterCutoff < BW_FASTEST * audio_->GetSampleRate() / 2);
	EarlyDownsample(isKaiserFast, nOctaves, audio_->GetSampleRate() / 2., filterCutoff);

	cqtResp_.reserve(nOctaves * qBasis_->GetFrequencies().size());
	const auto nFilters(min(static_cast<size_t>(octave), nBins));
#ifdef _DEBUG
	size_t nFft(0);
#endif
	if (not isKaiserFast)
	{
		// Do the top octave before resampling to allow for fast resampling
		qBasis_->CalcFilters(audio_->GetSampleRate(), fMinOctave, nFilters, sparsity);
#ifdef _DEBUG
		nFft = qBasis_->GetFftFrameLen();
#endif
		stft_ = make_unique<ShortTimeFourier>(qBasis_->GetFftFrameLen(),
			ShortTimeFourier::STFT_WINDOW::RECT, isPadReflect);
		Response();

		fMinOctave /= 2;
		fMaxOctave /= 2;
		nOctaves -= 1;
		filterCutoff = fMaxOctave * (1 + .5 * WIN_BAND_WIDTH[
			static_cast<int>(window)] / qBasis_->GetQfactor());
	}

	if (Num2factors(hopLen) < nOctaves - 1)
	{
		ostringstream os;
		os << "Hop length must be a positive integer, long enough, multiple of 2^" << nOctaves
			<< " = " << static_cast<int>(pow(2, nOctaves)) << " to support the bottom octave of "
			<< nOctaves << "-octave CQT";
		throw CqtError(os.str().c_str());
	}

	qBasis_->CalcFilters(audio_->GetSampleRate(), fMinOctave, nFilters, sparsity);
#ifdef _DEBUG
	assert(nFft == 0 or nFft == qBasis_->GetFftFrameLen() and
		"STFT frame length has changed, but it should not");
#endif
	if (not stft_) stft_ = make_unique<ShortTimeFourier>(qBasis_->GetFftFrameLen(),
		ShortTimeFourier::STFT_WINDOW::RECT, isPadReflect);

	const auto rateInitial(audio_->GetSampleRate());

	for (int i(0); i < nOctaves; ++i)
	{
		if (i) HalfDownSample(nOctaves); // except first time
		Response();
	}

	assert(cqtResp_.size() == nBins and "Wrong CQT-spectrum size");
	TrimErrors();
	Scale(rateInitial, fMin, toScale);

	// Eventually, we can flatten the array, and get rid of temporary 2D-buffer:
	cqt_.resize(cqtResp_.size() * cqtResp_.front().size());
	AlignedVector<float>::iterator unusedIter;
	for (size_t i(0); i < cqtResp_.size(); ++i) unusedIter = copy(cqtResp_.at(i).cbegin(),
		cqtResp_.at(i).cend(), cqt_.begin() + static_cast<ptrdiff_t>(i * cqtResp_.at(i).size()));

	MKL_Simatcopy('R', 'T', nBins, cqtResp_.front().size(), 1, cqt_.data(),
		max(1ull, cqtResp_.front().size()), max(1ull, nBins));

	stft_.reset(); // do not have to do it here, but will not need it anymore,
	// so we can release it, because it is not const, and it is "unique" pointer
	cqtResp_.clear();
}

ConstantQ::~ConstantQ() {}


void ConstantQ::EarlyDownsample(const bool isKaiserFast,
	const int nOctaves, const double nyquist, const double cutOff)
{
	if (not isKaiserFast) return;
	const auto nDownSampleOps = min(max(0, static_cast<int>(ceil(log2(
		BW_FASTEST * nyquist / cutOff)) - 1) - 1), max(0, Num2factors(hopLen_) - nOctaves + 1));
	if (nDownSampleOps == 0) return;

	const auto downSampleFactor(static_cast<int>(pow(2, nDownSampleOps)));
	hopLen_ /= downSampleFactor;
	if (audio_->GetNumSamples() < static_cast<size_t>(downSampleFactor))
	{
		ostringstream os;
		os << "Input audio signal length = " << audio_->GetNumSamples()
			<< " is too short for " << nOctaves << "-octave constant-q spectrum";
		throw CqtError(os.str().c_str());
	}
	
	audio_->MonoResample(audio_->GetSampleRate() / downSampleFactor);
}

void ConstantQ::HalfDownSample(const int nOctaves)
{
	if (audio_->GetNumSamples() < 2)
	{
		ostringstream os;
		os << "Input audio signal length = " << audio_->GetNumSamples()
			<< " is too short for " << nOctaves << "-octave constant-q spectrum";
		throw CqtError(os.str().c_str());
	}

	audio_->MonoResample(audio_->GetSampleRate() / 2);

	// Scale the resampled signal, so that it has approximately equal total energy:
	CHECK_IPP_RESULT(ippsMulC_32f_I(sqrtf(2), reinterpret_cast<Ipp32f*>(audio_->GetRawData()),
		static_cast<int>(audio_->GetNumSamples())));

	qBasis_->ScaleFilters(sqrtf(2)); // to compensate for downsampling
	hopLen_ /= 2;
}

void ConstantQ::Response()
{
	assert(cqtResp_.size() < nBins_ and "Wrong CQT-spectrum size");

	stft_->RealForward(reinterpret_cast<float*>(
		audio_->GetRawData()), audio_->GetNumSamples(), hopLen_);
	
	// Filter response energy:
	AlignedVector<MKL_Complex8> resp(qBasis_->GetLengths().size() * stft_->GetNumFrames());
	qBasis_->RowMajorMultiply(reinterpret_cast<const MKL_Complex8*>(
		stft_->GetSTFT().data()), resp.data(), static_cast<int>(stft_->GetNumFrames()));

	// Unfortunately, cannot fill flattened array straight away,
	// because we will know the final truncated number of frames
	// only after all down-samples are finished, so, now append to the 2D-stack:
	for (auto i(static_cast<ptrdiff_t>(qBasis_->GetLengths().size()) - 1); i > -1; --i)
	{
		cqtResp_.push_back(vector<float>(stft_->GetNumFrames()));
		CHECK_IPP_RESULT(ippsMagnitude_32fc(reinterpret_cast<Ipp32fc*>(resp.data()
			+ static_cast<ptrdiff_t>(i * cqtResp_.back().size())),
			cqtResp_.back().data(), static_cast<int>(cqtResp_.back().size())));

		if (cqtResp_.size() == nBins_) return; // Clip out bottom frequencies we do not want
	}

	assert(cqtResp_.size() < nBins_ and "Wrong CQT-spectrum size");
}

void ConstantQ::TrimErrors()
{
	// FFmpeg strangely loses small number of frames after each down-sample
	// And STFT right-end may get truncated several times,
	// so cleanup framing errors at the right-boundary:
	const auto minCol(min_element(cqtResp_.cbegin(), cqtResp_.cend(),
		[](const vector<float>& lhs, const vector<float>& rhs)
	{ return lhs.size() < rhs.size(); })->size());
	for (auto& resp : cqtResp_) resp.resize(minCol);

	reverse(cqtResp_.begin(), cqtResp_.end());
}

void ConstantQ::Scale(const int rateInit, const float fMin, const bool toScale)
{
	if (not toScale) return;

	qBasis_->CalcLengths(rateInit, fMin, cqtResp_.size());
	assert(qBasis_->GetLengths().size() == nBins_ and "Wrong number of CQT-lengths");
	CHECK_IPP_RESULT(ippsSqrt_32f_I(qBasis_->GetLengths().data(), static_cast<int>(nBins_)));

	for (size_t i(0); i < cqtResp_.size(); ++i) CHECK_IPP_RESULT(ippsDivC_32f_I(
		qBasis_->GetLengths().at(i), cqtResp_.at(i).data(), static_cast<int>(cqtResp_.at(i).size())));
}


void ConstantQ::Amplitude2power() { CHECK_IPP_RESULT(
	ippsSqr_32f_I(cqt_.data(), static_cast<int>(cqt_.size()))); }

void ConstantQ::TrimSilence(const float aMin, const float topDb)
{
	vector<Ipp32f> mse(cqt_.size() / nBins_); // Mean-square energy:
	for (size_t i(0); i < mse.size(); ++i) CHECK_IPP_RESULT(ippsMean_32f(cqt_.data()
		+ static_cast<ptrdiff_t>(i * nBins_), static_cast<int>(nBins_), &mse.at(i), ippAlgHintFast));

	Ipp32f mseMax;
	CHECK_IPP_RESULT(ippsMax_32f(mse.data(), static_cast<int>(mse.size()), &mseMax));
	Power2db_helper(mse.data(), static_cast<int>(mse.size()), mseMax, aMin, 0);

	const auto iterStart(find_if(mse.cbegin(), mse.cend(),
		[topDb](Ipp32f mse_i) { return mse_i > -topDb; }));
	const auto iterEnd(find_if(mse.crbegin(), mse.crend(),
		[topDb](Ipp32f mse_i) { return mse_i > -topDb; }));

	const auto unusedIter(cqt_.erase(cqt_.cbegin(),
		cqt_.cbegin() + (iterStart - mse.cbegin()) * static_cast<ptrdiff_t>(nBins_)));
	cqt_.resize(cqt_.size() - (iterEnd - mse.crbegin()) * static_cast<ptrdiff_t>(nBins_));
	assert(cqt_.size() % nBins_ == 0 and "CQT is not rectangular after silence trimming");
}

void ConstantQ::Power2db_helper(float* spectr, const int size,
	const float ref, const float aMin, const float topDb)
{
	assert(*min_element(spectr, spectr + size) >= 0 and
		"Did you forget to square CQT-amplitudes to convert them to power?");
	assert(ref >= 0 and aMin > 0 and "Reference and minimum powers must be strictly positive");

	// Scale power relative to 'ref' in a numerically stable way:
	// S_db = 10 * log10(S / ref) ~= 10 * log10(S) - 10 * log10(ref)
	// Zeros in the output correspond to positions where S == ref
	CHECK_IPP_RESULT(ippsThreshold_LT_32f_I(spectr, size, aMin));
	CHECK_IPP_RESULT(ippsLog10_32f_A11(spectr, spectr, size));
	CHECK_IPP_RESULT(ippsMulC_32f_I(10, spectr, size));
	CHECK_IPP_RESULT(ippsSubC_32f_I(10 * log10(max(aMin, ref)), spectr, size));

	assert(topDb >= 0 and "top_db must be non-negative");
	if (topDb) // Threshold the output at topDb below the peak:
	{
		Ipp32f maxDb;
		CHECK_IPP_RESULT(ippsMax_32f(spectr, size, &maxDb));
		CHECK_IPP_RESULT(ippsThreshold_LT_32f_I(spectr, size, maxDb - topDb));
	}
}