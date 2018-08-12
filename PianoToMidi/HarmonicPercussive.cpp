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

	vector<float> paddedBuff(((cqt->GetCQT().size() / cqt->GetNumBins())
		+ kernelHarm - 1) * cqt->GetNumBins());
	CHECK_IPP_RESULT(ippiCopyMirrorBorder_32f_C1R_L(cqt->GetCQT().data(),
		static_cast<int>(cqt->GetNumBins() * sizeof cqt->GetCQT().front()),
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(cqt->GetCQT().size()
			/ cqt->GetNumBins()) }, paddedBuff.data(), static_cast<int>(cqt->GetNumBins()
				* sizeof paddedBuff.front()), { static_cast<int>(cqt->GetNumBins()),
		static_cast<int>(paddedBuff.size() / cqt->GetNumBins()) }, kernelHarm / 2, 0));

	Ipp32u buffSize;
	CHECK_IPP_RESULT(ippiFilterMedianGetBufferSize_64f({ static_cast<int>(cqt->GetNumBins()),
		static_cast<int>(cqt->GetCQT().size() / cqt->GetNumBins()) }, { 1, kernelHarm }, 1, &buffSize));
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
		{ static_cast<int>(cqt->GetNumBins()), static_cast<int>(filtDouble.size() / cqt->GetNumBins()) },
		{ kernelPerc, 1 }, { 0, 0 }, buff.data()));

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