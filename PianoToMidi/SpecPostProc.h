#pragma once

class SpecPostProc abstract
{
public:
	static void Amplitude2power(AlignedVector<float>* spectr);
	static void TrimSilence(AlignedVector<float>* spectr, size_t nBins, float aMin = 1e-10f, float topDb = 60.f);
	static void Power2db(AlignedVector<float>* spectr, float ref = 1.f, float aMin = 1e-10f, float topDb = 80.f);
};