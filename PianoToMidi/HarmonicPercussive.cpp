#include "stdafx.h"
#include "HarmonicPercussive.h"
#include "AlignedVector.h"
#include "ConstantQ.h"

using std::shared_ptr;

HarmonicPercussive::HarmonicPercussive(const shared_ptr<ConstantQ>& cqt, const int kernel,
	const float power, const bool toMask, const float margin)
{
	UNREFERENCED_PARAMETER(cqt);
	UNREFERENCED_PARAMETER(kernel);
	UNREFERENCED_PARAMETER(power);
	UNREFERENCED_PARAMETER(toMask);
	UNREFERENCED_PARAMETER(margin);
}

HarmonicPercussive::~HarmonicPercussive()
{
}