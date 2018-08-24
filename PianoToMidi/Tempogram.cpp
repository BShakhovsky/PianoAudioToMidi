#include "stdafx.h"

#include "EnumFuncs.h"
#include "Tempogram.h"

#include "AlignedVector.h"
#include "HarmonicPercussive.h"
#include "IntelCheckStatus.h"

using namespace std;

void AutoCorrelate(vector<float>* srcDst)
{
	// Bounded (truncated) auto-correlation y*y along the second axis

	const auto sigSize(srcDst->size());
	// Pad out the signal with zeros to support full-length auto-correlation:
	srcDst->resize(srcDst->size() * 2 + 1, 0);

	int specSize, initSize, workSize;
	CHECK_IPP_RESULT(ippsDFTGetSize_R_32f(static_cast<int>(srcDst->size()),
		IPP_FFT_DIV_INV_BY_N, ippAlgHintFast, &specSize, &initSize, &workSize));
	const unique_ptr<byte[]> spec(new byte[static_cast<size_t>(specSize)]);
	vector<Ipp8u> initBuf(static_cast<size_t>(initSize)), workBuf(static_cast<size_t>(workSize));

	CHECK_IPP_RESULT(ippsDFTInit_R_32f(static_cast<int>(srcDst->size()),
		IPP_FFT_DIV_INV_BY_N, ippAlgHintFast,
		reinterpret_cast<IppsDFTSpec_R_32f*>(spec.get()), initBuf.data()));

	srcDst->emplace_back(0.f); // because we have odd length
	CHECK_IPP_RESULT(ippsDFTFwd_RToCCS_32f(srcDst->data(), srcDst->data(),
		reinterpret_cast<IppsDFTSpec_R_32f*>(spec.get()), workBuf.data())); // Power spectrum

	for (size_t i(0); i < srcDst->size() - 1; i += 2)
	{
		srcDst->at(i) = srcDst->at(i) * srcDst->at(i) + srcDst->at(i + 1) * srcDst->at(i + 1);
		srcDst->at(i + 1) = 0;
	}

	CHECK_IPP_RESULT(ippsDFTInv_CCSToR_32f(srcDst->data(), srcDst->data(),
		reinterpret_cast<IppsDFTSpec_R_32f*>(spec.get()), workBuf.data())); // Convert back to time domain
	srcDst->resize(sigSize);
}

void Tempogram::Calculate(const vector<float>& oEnv, const int winLen, const bool toCenter,
	const WIN_FUNC window, const NORM_TYPE norm)
{
	/* Local (localized) autocorrelation of the onset strength envelope
		Grosche, Peter, Meinard Müller, and Frank Kurth.
		"Cyclic tempogram - A mid-level tempo representation for music signals."
		ICASSP, 2010 */

	assert(not oEnv.empty() and
		"Onset strength envelope must be calculated prior to estimating tempo");

	// The default length of the onset autocorrelation window
	// (384 in frames / onset measurements) corresponds to 384 * hop_length / sr ~= 8.9 seconds
	assert(winLen > 0 and "Window length must be positive and non-zero");

	vector<float> paddedEnvelope;
	vector<float>::iterator unusedIter;
	if (toCenter)
	{
		paddedEnvelope.resize(oEnv.size() + winLen);
		unusedIter = copy(oEnv.cbegin(), oEnv.cend(), paddedEnvelope.begin() + winLen / 2);
		// "Linear ramp" padding mode, with zero end value:
		for (int i(0); i < winLen / 2; ++i) paddedEnvelope.at(
			static_cast<size_t>(i)) = oEnv.front() / (winLen / 2) * i;
		for (int i(0); i < winLen / 2 + winLen % 2; ++i) paddedEnvelope.at(
			paddedEnvelope.size() - i - 1) = oEnv.back() / (winLen / 2 + winLen % 2) * i;
	}
	else paddedEnvelope = oEnv; // windows are left-aligned

	if (paddedEnvelope.size() < static_cast<size_t>(winLen))
	{
		autoCorr2D_.clear();
		return;
	}

	// If accidentally get additional frames, truncate to the length of the original signal:
	autoCorr2D_.assign(min(oEnv.size(), paddedEnvelope.size() - winLen),
		vector<float>(static_cast<size_t>(winLen)));

	auto WinFunc(GetWindowFunc(window, static_cast<size_t>(winLen)));
	auto NormFunc(GetNormFuncReal(norm));
	// Carve onset envelope into frames:
	for (size_t i(0); i < autoCorr2D_.size(); ++i) // hop length = 1
	{
		unusedIter = copy(paddedEnvelope.cbegin() + static_cast<ptrdiff_t>(i),
			paddedEnvelope.cbegin() + static_cast<ptrdiff_t>(i + winLen), autoCorr2D_.at(i).begin());
		WinFunc->multiplyWithWindowingTable(autoCorr2D_.at(i).data(), autoCorr2D_.at(i).size());
		AutoCorrelate(&autoCorr2D_.at(i));

		Ipp32f norm32(0);
		CHECK_IPP_RESULT(NormFunc(autoCorr2D_.at(i).data(),
			static_cast<int>(autoCorr2D_.at(i).size()), &norm32));
		if (norm32) CHECK_IPP_RESULT(ippsDivC_32f_I(norm32,
			autoCorr2D_.at(i).data(), static_cast<int>(autoCorr2D_.at(i).size())));
	}
}

float Tempogram::MostProbableTempo(const vector<float>& oEnv, const int rate, const int hopLen,
	const int startBpm, const float stdBpm, const float acSize,
	const float maxTempo, const AGGREGATE aggr)
{
	assert(startBpm > 0 and "Start BPM must be positive and non-zero");
	assert(acSize > 0 and "Length in seconds of the auto-correlation window must be > 0");

	Calculate(oEnv, static_cast<int>(acSize * rate / hopLen));
	if (autoCorr2D_.empty()) return 0; // audio is too short

	// If want to estimate time-varying tempo independently for each frame,
	// just do not aggregate, but now we need only average tempo:
	AlignedVector<float> autoCorrFlat(autoCorr2D_.size() * autoCorr2D_.front().size());
	for (size_t i(0); i < autoCorr2D_.size(); ++i) const auto unusedIter(copy(
		autoCorr2D_.at(i).cbegin(), autoCorr2D_.at(i).cend(),
		autoCorrFlat.begin() + static_cast<ptrdiff_t>(i * autoCorr2D_.at(i).size())));
	MKL_Simatcopy('R', 'T', autoCorr2D_.size(), autoCorr2D_.front().size(),
		1, autoCorrFlat.data(), autoCorr2D_.front().size(), autoCorr2D_.size());
	vector<float> tempos(autoCorr2D_.front().size());
	Aggregate(autoCorrFlat.data(), static_cast<int>(autoCorr2D_.front().size()), 0,
		static_cast<int>(autoCorr2D_.size()), tempos.data(), aggr);

	// Bin frequencies, corresponding to an onset auto-correlation or tempogram matrix:
	vector<float> bpms(tempos.size()), prior(tempos.size(), 0);
	bpms.front() = 0; // zero-lag bin skipped
	for (size_t i(1); i < prior.size(); ++i)
	{
		bpms.at(i) = 60 * rate / static_cast<float>(hopLen * i);
		// Kill everything above the max tempo:
		if (maxTempo > numeric_limits<float>::epsilon() and bpms.at(i) <= maxTempo)
			// Weight the autocorrelation by a log-normal distribution:
			prior.at(i) = tempos.at(i) * exp(-.5f * pow((log2(bpms.at(i))
				- log2(static_cast<float>(startBpm))) / stdBpm, 2));
	}

	/* Really, instead of multiplying by the prior, we should set up a probabilistic model
	for tempo and add log-probabilities. This would give us a chance to recover
	from null signals and rely on the prior. It would also make time aggregation much more natural. */

	// Maximum weighted by the prior:
	const auto bestPeriod(max_element(prior.cbegin(), prior.cend()) - prior.cbegin());
	return bestPeriod ? bpms.at(static_cast<size_t>(bestPeriod)) : startBpm; // if bestTempo is index zero
}