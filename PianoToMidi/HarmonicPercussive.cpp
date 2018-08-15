#include "stdafx.h"

#include "AlignedVector.h"
#include "HarmonicPercussive.h"
#include "ConstantQ.h"

#include "IntelCheckStatus.h"

using namespace std;

void SoftMask(vector<float>* xInOut, const vector<float>& xRef, float power, bool splitZeros)
{
	assert(xInOut->size() == xRef.size() and "Input and reference arrays must have the same size");
	assert(all_of(xInOut->cbegin(), xInOut->cend(), bind2nd(greater_equal<float>(), 0.f))
		and "Input mask elements must be non-negative");
	assert(all_of(xRef.cbegin(), xRef.cend(), bind2nd(greater_equal<float>(), 0.f))
		and "Background reference elements must be non-negative");
	assert(power > 0 and "Exponent for the Wiener filter must be strictly positive");

	if (power == numeric_limits<float>::infinity()) const auto unusedIter(transform(
		xInOut->cbegin(), xInOut->cend(), xRef.cbegin(), xInOut->begin(),
		greater<float>())); // Hard (binary) mask, X > X_ref, ties broken in favor of X_ref (mask=0)
	else const auto unusedIter(transform(xInOut->cbegin(), xInOut->cend(),
		xRef.cbegin(), xInOut->begin(), [&power, &splitZeros](float x, float ref)
	{
		if (max(x, ref) > numeric_limits<float>::epsilon())
		{
			// Robustly compute M = X^power / (X^power + X_ref^power) in a numerically stable way
			// (re-scale the input arrays relative to the larger value):
			const auto z(max(x, ref));
			x = pow(x / z, power);
			return x / (x + pow(ref / z, power));
		}
		else return splitZeros ? .5f : 0.f; // Wherever both energies close to zero, split the mask
	}));
}

HarmonicPercussive::HarmonicPercussive(const shared_ptr<ConstantQ>& cqt,
	const int kernelHarm, const int kernelPerc, const float power,
	const float margHarm, const float margPerc)
	: cqt_(cqt)
{
	/*	1. Fitzgerald, Derry.
			"Harmonic/percussive separation using median filtering."
			13th International Conference on Digital Audio Effects(DAFX10),
			Graz, Austria, 2010.
		2. Driedger, Müller, Disch.
			"Extending harmonic-percussive separation of audio."
			15th International Society for Music Information Retrieval Conference(ISMIR 2014),
			Taipei, Taiwan, 2014.

		If margin > 1, S = H + P + Residual */

	assert(min(margHarm, margPerc) >= 1 and "HPSS margins must be >= 1.0, a typical range is [1...10]");

	vector<float> paddedBuff(((cqt->GetCQT().size()
		/ cqt->GetNumBins()) + kernelHarm - 1) * cqt->GetNumBins());
	CHECK_IPP_RESULT(ippiCopyMirrorBorder_32f_C1R_L(cqt->GetCQT().data(),
		static_cast<int>(cqt->GetNumBins() * sizeof cqt->GetCQT().front()),
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(cqt->GetCQT().size()
			/ cqt->GetNumBins()) }, paddedBuff.data(), static_cast<int>(cqt->GetNumBins()
				* sizeof paddedBuff.front()), { static_cast<int>(cqt->GetNumBins()),
		static_cast<int>(paddedBuff.size() / cqt->GetNumBins()) }, kernelHarm / 2, 0));

	Ipp32u buffSize;
	CHECK_IPP_RESULT(ippiFilterMedianGetBufferSize_64f({ static_cast<int>(cqt->GetNumBins()),
		static_cast<int>(cqt->GetCQT().size() / cqt->GetNumBins()) },
		{ 1, kernelHarm }, 1, &buffSize));
	vector<Ipp8u> buff(buffSize);

	vector<double> filtDouble(cqt->GetCQT().size()),
		paddedDouble(paddedBuff.cbegin(), paddedBuff.cend());
	CHECK_IPP_RESULT(ippiFilterMedian_64f_C1R(paddedDouble.data(),
		static_cast<int>(cqt->GetNumBins() * sizeof paddedDouble.front()), filtDouble.data(),
		static_cast<int>(cqt->GetNumBins() * sizeof filtDouble.front()),
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(filtDouble.size()
			/ cqt->GetNumBins()) }, { 1, kernelHarm }, { 0, 0 }, buff.data()));

	harm_.resize(filtDouble.size());
	auto unusedIter(transform(filtDouble.cbegin(), filtDouble.cend(), harm_.begin(),
		[](double val) { return static_cast<float>(val); }));


	paddedBuff.resize(cqt->GetCQT().size() / cqt->GetNumBins()
		* (cqt->GetNumBins() + kernelPerc - 1));
	CHECK_IPP_RESULT(ippiCopyMirrorBorder_32f_C1R_L(cqt->GetCQT().data(),
		static_cast<int>(cqt->GetNumBins() * sizeof cqt->GetCQT().front()),
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(cqt->GetCQT().size()
			/ cqt->GetNumBins()) }, paddedBuff.data(), static_cast<int>((cqt->GetNumBins()
				+ kernelPerc - 1) * sizeof paddedBuff.front()),
		{ static_cast<int>(cqt->GetNumBins() + kernelPerc - 1),
		static_cast<int>(cqt->GetCQT().size() / cqt->GetNumBins()) }, 0, kernelPerc / 2));

	CHECK_IPP_RESULT(ippiFilterMedianGetBufferSize_64f({ static_cast<int>(cqt->GetNumBins()),
		static_cast<int>(cqt->GetCQT().size() / cqt->GetNumBins()) },
		{ kernelPerc, 1 }, 1, &buffSize));
	buff.resize(buffSize);

	assert(filtDouble.size() == cqt->GetCQT().size() and
		"Harmonic & percussive matrix sizes must be equal");
	paddedDouble.assign(paddedBuff.cbegin(), paddedBuff.cend());
	CHECK_IPP_RESULT(ippiFilterMedian_64f_C1R(paddedDouble.data(),
		static_cast<int>((cqt->GetNumBins() + kernelPerc - 1) * sizeof paddedDouble.front()),
		filtDouble.data(), static_cast<int>(cqt->GetNumBins() * sizeof filtDouble.front()),
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(filtDouble.size()
			/ cqt->GetNumBins()) }, { kernelPerc, 1 }, { 0, 0 }, buff.data()));

	perc_.resize(filtDouble.size());
	unusedIter = transform(filtDouble.cbegin(), filtDouble.cend(), perc_.begin(),
		[](double val) { return static_cast<float>(val); });

	auto refHarm(harm_), refPerc(perc_);
	CHECK_IPP_RESULT(ippsMulC_32f_I(margHarm, refPerc.data(), static_cast<int>(refPerc.size())));
	CHECK_IPP_RESULT(ippsMulC_32f_I(margPerc, refHarm.data(), static_cast<int>(refHarm.size())));
	SoftMask(&harm_, refPerc, power, margHarm == 1 and margPerc == 1);
	SoftMask(&perc_, refHarm, power, margHarm == 1 and margPerc == 1);

	CHECK_IPP_RESULT(ippsMul_32f_I(cqt->GetCQT().data(), harm_.data(), static_cast<int>(harm_.size())));
	CHECK_IPP_RESULT(ippsMul_32f_I(cqt->GetCQT().data(), perc_.data(), static_cast<int>(perc_.size())));
}

void HarmonicPercussive::OnsetEnvelope(const size_t lag, const int maxSize,
	const bool toDetrend, const bool toCenter, const AGGREGATE aggr) //centering = None
{
	/* mean_f max(0, S[f, t] - Sref[f, t - lag])
		where Sref = S after local max filtering along the frequency axis

	Böck, Sebastian, and Gerhard Widmer.
	"Maximum filter vibrato suppression for onset detection."
	16th International Conference on Digital Audio Effects,
	Maynooth, Ireland. 2013 */

	assert(lag >= 1 and "Onset strength envelope lag must be >= 1");
	assert(maxSize >= 1 and "Onset strength envelope max size must be >= 1");

	// Difference to the reference, spaced by lag (S[f, t] - Sref[f, t - lag]):
	vector<float> percDiff(perc_.cbegin()
		+ static_cast<ptrdiff_t>(lag * cqt_->GetNumBins()), perc_.cend());
	vector<float> percRef(perc_.cbegin(), perc_.cend()
		- static_cast<ptrdiff_t>(lag * cqt_->GetNumBins()));
	if (maxSize > 1)
	{
		int buffSize;
		CHECK_IPP_RESULT(ippiFilterMaxBorderGetBufferSize({ static_cast<int>(cqt_->GetNumBins()),
			static_cast<int>(percDiff.size() / cqt_->GetNumBins()) },
			{ maxSize, 1 }, ipp32f, 1, &buffSize));
		vector<Ipp8u> buff(static_cast<size_t>(buffSize));

		CHECK_IPP_RESULT(ippiFilterMaxBorder_32f_C1R(percRef.data(),
			static_cast<int>(cqt_->GetNumBins() * sizeof percRef.front()), percRef.data(),
			static_cast<int>(cqt_->GetNumBins() * sizeof percRef.front()),
			{ static_cast<int>(cqt_->GetNumBins()), static_cast<int>(percRef.size()
				/ cqt_->GetNumBins()) }, { maxSize, 1 }, ippBorderConst, 0, buff.data()));
	}
	CHECK_IPP_RESULT(ippsSub_32f_I(percRef.data(),
		percDiff.data(), static_cast<int>(percDiff.size())));
	CHECK_IPP_RESULT(ippsThreshold_LT_32f_I(percDiff.data(),
		static_cast<int>(percDiff.size()), 0)); // Discard negatives (decreasing amplitude)

	vector<float>::iterator unusedIter;
	percEnv_.resize(percDiff.size() / cqt_->GetNumBins());
	IppStatus (*AggrFunc)(const Ipp32f* src, int len, Ipp32f* resMeanMinMax);
	AggrFunc = nullptr;
	switch (aggr) // combining onsets at different frequency bins:
	{
	case AGGREGATE::MEAN:	AggrFunc = [](const Ipp32f*pSrc, int len, Ipp32f* pMin)
		{ return ippsMean_32f(pSrc, len, pMin, ippAlgHintFast); };	break;
	case AGGREGATE::MIN:	AggrFunc = &ippsMin_32f;				break;
	case AGGREGATE::MAX:	AggrFunc = &ippsMax_32f;				break;
	case AGGREGATE::MEDIAN:
	{
		vector<double> percEnvDouble(percEnv_.size()),
			percDiffDouble(percDiff.cbegin(), percDiff.cend());

		Ipp32u buffSize;
		CHECK_IPP_RESULT(ippiFilterMedianGetBufferSize_64f({ 1,
			static_cast<int>(percEnvDouble.size()) }, { static_cast<int>(cqt_->GetNumBins()
				+ cqt_->GetNumBins() % 2 - 1), 1 }, 1, &buffSize));
		vector<Ipp8u> buff(buffSize);

		CHECK_IPP_RESULT(ippiFilterMedian_64f_C1R(percDiffDouble.data(),
			static_cast<int>(cqt_->GetNumBins() * sizeof percDiffDouble.front()),
			percEnvDouble.data(), static_cast<int>(sizeof percEnvDouble.front()), { 1,
			static_cast<int>(percEnvDouble.size()) }, { static_cast<int>(cqt_->GetNumBins()
				+ cqt_->GetNumBins() % 2 - 1), 1 }, { 0, 0 }, buff.data()));

		unusedIter = transform(percEnvDouble.cbegin(), percEnvDouble.cend(),
			percEnv_.begin(), [](double val) { return static_cast<float>(val); });
	} break;
	default: assert(!"Not all aggregating functions checked");
	}
	if (AggrFunc) for (size_t i(lag); i < percEnv_.size(); ++i) CHECK_IPP_RESULT(
		AggrFunc(percDiff.data() + static_cast<ptrdiff_t>(i * cqt_->GetNumBins()),
			static_cast<int>(cqt_->GetNumBins()), &percEnv_.at(i)));

	unusedIter = percEnv_.insert(percEnv_.begin(), lag // compensate for lag
		+ (toCenter ? // Shift to counter-act framing effects:
			cqt_->GetFftFrameLength() / 2 / cqt_->GetHopLength() : 0), 0);

	if (toDetrend)	// Remove the DC component by Direct Infinite Impulse Response (IIR) filter
	{				// (transposed implementation of standard difference equation)
		int buffSize;
		CHECK_IPP_RESULT(ippsIIRGetStateSize_32f(1, &buffSize));
		vector<Ipp8u> buff(static_cast<size_t>(buffSize));

		IppsIIRState_32f* state;
		const vector<Ipp32f> taps({ 1, -1, 1, -.99f });
		CHECK_IPP_RESULT(ippsIIRInit_32f(&state, taps.data(), 1, nullptr, buff.data()));
		CHECK_IPP_RESULT(ippsIIR_32f_I(percEnv_.data(), static_cast<int>(percEnv_.size()), state));
	}

	if (toCenter) percEnv_.resize(perc_.size() / cqt_->GetNumBins()); // Trim to match the input duration
}

void HarmonicPercussive::OnsetPeaksDetect(const bool toBackTrack)
{
	percPeaks_.clear();
	// Do we have any onsets to grab?
	if (all_of(percEnv_.cbegin(), percEnv_.cend(), bind1st(equal_to<float>(), 0.f))) return;

	// If onset envelope calculated with toDetrend=true, IIR filter may produce negative values.
	// So, shift up (a common normalization step to make the threshold more consistent):
	Ipp32f percEnvMinVal, percEnvMaxVal;
	CHECK_IPP_RESULT(ippsMinMax_32f(percEnv_.data(), static_cast<int>(percEnv_.size()),
		&percEnvMinVal, &percEnvMaxVal));
	CHECK_IPP_RESULT(ippsNormalize_32f_I(percEnv_.data(), static_cast<int>(percEnv_.size()),
		percEnvMinVal, percEnvMaxVal - percEnvMinVal)); // normalize to [0, 1] range
#ifdef _DEBUG
	const auto minMax(minmax_element(percEnv_.cbegin(), percEnv_.cend()));
	assert(*minMax.first >= 0 and *minMax.first < numeric_limits<float>::epsilon()
		and "Onset envelope shifted incorrectly");
	assert (*minMax.second > 1 - numeric_limits<float>::epsilon() and *minMax.second <= 1
		and "Onset envelope not normalized");
#endif

	/* Flexible heuristic with the following three conditions:
		1. x[n] == max(x[n - preMax : n + postMax])
		2. x[n] >= mean(x[n - preAvg : n + postAvg]) + delta
		3. n - previous_n (last peak) > wait (greedy)

		Where parameter settings found by large-scale hyper-parameter optimization
		over the dataset from https://github.com/CPJKU/onset_db

		1. Boeck, Sebastian, Florian Krebs, and Markus Schedl.
			"Evaluating the Online Capabilities of Onset Detection Methods." ISMIR, 2012.
		2. https://github.com/CPJKU/onset_detection/blob/master/onset_program.py */

	int maxLen(static_cast<int>(ceil(.03 * cqt_->GetSampleRate() / cqt_->GetHopLength()))); // 30ms
	maxLen += maxLen % 2 + 1; // odd filter size, just in case
	vector<float> percEnvMax(percEnv_.size() + maxLen / 2); // maximums over a sliding window
	// Shift right, so that anchor point is not center but right-end:
	const auto unusedIter(copy(percEnv_.cbegin(), percEnv_.cend(), percEnvMax.begin() + maxLen / 2));

	int buffSize;
	CHECK_IPP_RESULT(ippiFilterMaxBorderGetBufferSize(
		{ static_cast<int>(percEnvMax.size()), 1 }, { maxLen, 1 }, ipp32f, 1, &buffSize));
	vector<Ipp8u> buff(static_cast<size_t>(buffSize));
	const Ipp32f borderVal(0);	// 'Constant' mode with value = 0 (x.min())
	// effectively truncates the maximum sliding window at the boundaries
	CHECK_IPP_RESULT(ippiFilterMaxBorder_32f_C1R(percEnvMax.data(),
		static_cast<int>(percEnvMax.size() * sizeof percEnvMax.front()), percEnvMax.data(),
		static_cast<int>(percEnvMax.size() * sizeof percEnvMax.front()), { static_cast<int>(
			percEnvMax.size()), 1 }, { maxLen, 1 }, ippBorderConst, borderVal, buff.data()));
	percEnvMax.resize(percEnv_.size()); // previously was shifted right, now truncate from right


	// 200ms (100ms before & after):
	auto avgLen(static_cast<int>(ceil(.2f * cqt_->GetSampleRate() / cqt_->GetHopLength())));
	avgLen += avgLen % 2 + 1; // odd filter size, just in case
	vector<float> percEnvSum(percEnv_); // Sums (--> means) over a sliding window
	// No need to shift right here, because central anchor point is what we need

	CHECK_IPP_RESULT(ippiSumWindowGetBufferSize(
		{ static_cast<int>(percEnvSum.size()), 1 }, { avgLen, 1 }, ipp32f, 1, &buffSize));
	buff.resize(static_cast<size_t>(buffSize));
	CHECK_IPP_RESULT(ippiSumWindow_32f_C1R(percEnvSum.data(),
		static_cast<int>(percEnvSum.size() * sizeof percEnvSum.front()), percEnvSum.data(),
		static_cast<int>(percEnvSum.size() * sizeof percEnvSum.front()), { static_cast<int>(
			percEnvSum.size()), 1 }, { avgLen, 1 }, ippBorderConst, &borderVal, buff.data()));

	const auto wait(static_cast<int>(ceil(.03f * cqt_->GetSampleRate() / cqt_->GetHopLength())));
	for (size_t i(0); i < percEnv_.size(); ++i) // Greedily remove onsets closer than 30ms:
		if ((percPeaks_.empty() or i > percPeaks_.back() + wait)
			and percEnv_.at(i) == percEnvMax.at(i) // Mask out entries not equal to the local max
			// Then mask out all entries less than the thresholded average:
			and percEnv_.at(i) - percEnvSum.at(i) / ( // Correct sliding average
				// in the ranges where the window needs to be truncated:
				min(avgLen / 2, static_cast<int>(i)) // at the beginning
				+ min(avgLen / 2, static_cast<int>(percEnv_.size() - i - 1)) // at the end
				+ 1) >= .07f) percPeaks_.emplace_back(i);

	if (toBackTrack) OnsetBackTrack();
}

void HarmonicPercussive::OnsetBackTrack()
{
	/* Roll back onset events from a peak amplitude to the nearest preceding energy minimum.
	Primarily useful when using onsets as slice points for segmentation, as described by:
	Jehan, Tristan. "Creating music by listening"
	Doctoral dissertation
	Massachusetts Institute of Technology, 2005. */

	assert(not percPeaks_.empty() and "Attempting to back-track empty onsets array");
	size_t i(percPeaks_.back()), j(percPeaks_.size() - 1);
	if (i-- == 0) return;

	for (; i; --i) if (percEnv_.at(i) <= percEnv_.at(i - 1) // Energy non-increasing
		and percEnv_.at(i) < percEnv_.at(i + 1))
	{
		for (; j and percPeaks_.at(j) > i; --j) percPeaks_.at(j) = i;
		if (j == 0 and percPeaks_.front() > i) break;
		i = percPeaks_.at(j) + 1;
	}
	for (size_t k(0); k <= j; ++k) percPeaks_.at(k) = i;
}