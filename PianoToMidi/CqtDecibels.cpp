#include "stdafx.h"
#include "CqtDecibels.h"
#include "IntelCheckStatus.h"

void CqtDecibels::Power2Db(float* spectr, const int size,
	const float ref, const float aMin, const float topDb)
{
	using namespace std;

	assert(*min_element(spectr, spectr + size) >= 0 and
		"Power can only be positive, because it is squared amplitude");
	assert(ref >= 0 and aMin > 0 and "Reference and minimum powers must be strictly positive");

	// Scale power relative to 'ref' in a numerically stable way:
	// S_db = 10 * log10(S / ref) ~= 10 * log10(S) - 10 * log10(ref)
	// Zeros in the output correspond to positions where S == ref

	CHECK_IPP_RESULT(ippiThreshold_32f_C1IR(spectr, size, { size, 1 }, aMin, ippCmpLess));
	CHECK_IPP_RESULT(ippsLog10_32f_A11(spectr, spectr, size));
	CHECK_IPP_RESULT(ippsMulC_32f_I(10, spectr, size));
	CHECK_IPP_RESULT(ippsSubC_32f_I(10 * log10(max(aMin, ref)), spectr, size));

	assert(topDb >= 0 and "top_db must be non-negative");
	if (topDb) // Threshold the output at topDb below the peak:
	{
		Ipp32f maxDb;
		CHECK_IPP_RESULT(ippsMax_32f(spectr, size, &maxDb));
		CHECK_IPP_RESULT(ippiThreshold_32f_C1IR(spectr, size, { size, 1 }, maxDb - topDb, ippCmpLess));
	}
}

void CqtDecibels::Amplitude2Db(float* spectr, const int size,
	const float ref, const float aMin, const float topDb)
{
	// Power2Db(S ^ 2), provided for convenience
	CHECK_IPP_RESULT(ippsSqr_32f_I(spectr, size));
	Power2Db(spectr, size, ref * ref, aMin * aMin, topDb);
}