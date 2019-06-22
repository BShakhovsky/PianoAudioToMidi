#include "stdafx.h"
#include "AlignedVector.h"
#include "SpecPostProc.h"
#include "IntelCheckStatus.h"

using namespace std;

void SpecPostProc::Amplitude2power(AlignedVector<float>* spectr)
{
	CHECK_IPP_RESULT(ippsSqr_32f_I(spectr->data(), static_cast<int>(spectr->size())));
}

void SpecPostProc::TrimSilence(AlignedVector<float>* spectr, const size_t nBins, const float aMin, const float topDb)
{
	AlignedVector<Ipp32f> mse(spectr->size() / nBins); // Mean-square energy:
	for (size_t i(0); i < mse.size(); ++i) CHECK_IPP_RESULT(ippsMean_32f(spectr->data() + static_cast<ptrdiff_t>(i * nBins), static_cast<int>(nBins), &mse.at(i), ippAlgHintFast));

	Ipp32f mseMax;
	CHECK_IPP_RESULT(ippsMax_32f(mse.data(), static_cast<int>(mse.size()), &mseMax));
	Power2db(&mse, mseMax, aMin, 0);

	const auto iterStart(find_if(mse.cbegin(), mse.cend(), [topDb](Ipp32f mse_i) { return mse_i > -topDb; }));
	const auto iterEnd(find_if(mse.crbegin(), mse.crend(), [topDb](Ipp32f mse_i) { return mse_i > -topDb; }));

	const auto unusedIter(spectr->erase(spectr->cbegin(), spectr->cbegin() + (iterStart - mse.cbegin()) * static_cast<ptrdiff_t>(nBins)));
	spectr->resize(spectr->size() - (iterEnd - mse.crbegin()) * static_cast<ptrdiff_t>(nBins));
	assert(spectr->size() % nBins == 0 and "Spectrum is not rectangular after silence trimming");
}

void SpecPostProc::Power2db(AlignedVector<float>* spectr, const float ref, const float aMin, const float topDb)
{
	assert(*min_element(spectr->data(), spectr->data() + static_cast<ptrdiff_t>(spectr->size())) >= 0 and
		"Did you forget to square CQT-amplitudes to convert them to power?");
	assert(ref >= 0 and aMin > 0 and "Reference and minimum powers must be strictly positive");

	// Scale power relative to 'ref' in a numerically stable way:
	// S_db = 10 * log10(S / ref) ~= 10 * log10(S) - 10 * log10(ref)
	// Zeros in the output correspond to positions where S == ref
	CHECK_IPP_RESULT(ippsThreshold_LT_32f_I(spectr->data(), static_cast<int>(spectr->size()), aMin));
	CHECK_IPP_RESULT(ippsLog10_32f_A11(spectr->data(), spectr->data(), static_cast<Ipp32s>(spectr->size())));
	CHECK_IPP_RESULT(ippsMulC_32f_I(10, spectr->data(), static_cast<int>(spectr->size())));
	CHECK_IPP_RESULT(ippsSubC_32f_I(10 * log10(max(aMin, ref)), spectr->data(), static_cast<int>(spectr->size())));

	assert(topDb >= 0 and "top_db must be non-negative");
	if (topDb) // Threshold the output at topDb below the peak:
	{
		Ipp32f maxDb;
		CHECK_IPP_RESULT(ippsMax_32f(spectr->data(), static_cast<int>(spectr->size()), &maxDb));
		CHECK_IPP_RESULT(ippsThreshold_LT_32f_I(spectr->data(), static_cast<int>(spectr->size()), maxDb - topDb));
	}
}