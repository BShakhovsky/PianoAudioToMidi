#include "stdafx.h"
#include "ConstantQ.h"
#include "AudioLoader.h"

#include "AlignedVector.h"
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

ConstantQ::ConstantQ(const shared_ptr<class AudioLoader>& audio, const int hopLen,
	const float fMin, const size_t nBins, const int octave, const float filtScale, const NORM_TYPE norm,
	const float sparsity, const CQT_WINDOW window, const bool toScale, const bool isPadReflect)
	: qBasis_(make_unique<CqtBasis>(octave, filtScale, norm, window)),
	audio_(audio), hopLen_(hopLen)
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
	EarlyDownsample(isKaiserFast, nOctaves, audio_->GetSampleRate() / 2., filterCutoff, toScale);

	cqtResp_.reserve(static_cast<size_t>(nOctaves));
	const auto nFilters(min(static_cast<size_t>(octave), nBins));
	if (not isKaiserFast)
	{
		// Do the top octave before resampling to allow for fast resampling
		qBasis_->CalcFilters(audio_->GetSampleRate(), fMinOctave, nFilters, sparsity);
		Response(isPadReflect);

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

	const auto rateInitial(audio_->GetSampleRate());

	for (int i(0); i < nOctaves; ++i)
	{
		if (i)
		{
			if (audio_->GetNumSamples() < 2)
			{
				ostringstream os;
				os << "Input audio signal length = " << audio_->GetNumSamples()
					<< " is too short for " << nOctaves << "-octave constant-q spectrum";
				throw CqtError(os.str().c_str());
			}
			audio_->MonoResample(audio_->GetSampleRate() / 2); // except first time
			qBasis_->ScaleFilters(sqrtf(2)); // to compensate for downsampling
			hopLen_ /= 2;
		}

		Response(isPadReflect);
	}

	TrimStack(nBins);
	Scale(rateInitial, fMin, nBins, toScale);
}

ConstantQ::~ConstantQ() {}

void ConstantQ::EarlyDownsample(const bool isKaiserFast, const int nOctaves,
	const double nyquist, const double cutOff, const bool toScale)
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

	// If not going to length-scale after CQT, need to compensate for the downsampling factor here:
	if (not toScale) CHECK_IPP_RESULT(ippsMulC_32f_I(sqrt(static_cast<Ipp32f>(downSampleFactor)),
		reinterpret_cast<Ipp32f*>(audio_->GetRawData()), static_cast<int>(audio_->GetNumSamples())));
}

void ConstantQ::Response(const bool isPadReflect) const
{
//	const ShortTimeFourier D(reinterpret_cast<float*>(audio_->GetRawData()), audio_->GetNumSamples(),
//		qBasis_->GetFftFrameLen(), hopLen_, ShortTimeFourier::STFT_WINDOW::RECT, isPadReflect);

	UNREFERENCED_PARAMETER(isPadReflect);

	// Filter response energy:
//	qBasis_->RowMajorMultiply(D.data(), dest, D->nDestColumns(nFrames));

	// Append to the stack
//	cqtResp_.push_back(dest);
}

void ConstantQ::TrimStack(const size_t nBins)
{
	// Cleanup any framing errors at the boundaries:
//	const auto maxCol = min(x.shape[1] for x in cqt_resp);
//	print(max_col, max(x.shape[1] for x in cqt_resp));

//	cqt_resp = vstack([x[:, : max_col] for x in cqt_resp][:: - 1]);
//	print(cqt_resp.shape);

	// Clip out bottom frequencies we do not want:
//	cqt_ = cqtResp[-n_bins:]
//	print(C.shape);

	UNREFERENCED_PARAMETER(nBins);
	
	cqtResp_.clear();
}

void ConstantQ::Scale(const int rateInit, const float fMin, const size_t nBins, const bool toScale) const
{
//	assert(cqt_.size() == qBasis_->GetLengths().size() and "Wrong CQT-spectrum size");
	if (not toScale) return;

	qBasis_->CalcLengths(rateInit, fMin, nBins);
	CHECK_IPP_RESULT(ippsSqrt_32f_I(qBasis_->GetLengths().data(),
		static_cast<int>(qBasis_->GetLengths().size())));

//	for (const auto& len : qBasis_->GetLengths()) cout << len << ' '; cout << endl;

//	for (size_t i(0); i < cqt_.size(); ++i) CHECK_IPP_RESULT(ippsDivC_32f_I(
//		qBasis_->GetLengths().at(i), cqt_.at(i).data(), static_cast<int>(cqt_.at(i).size())));
}