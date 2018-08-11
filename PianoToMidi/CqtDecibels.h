#pragma once

class CqtDecibels abstract
{
public:
	static void Power2Db(float* spectrInOut, int size,
		float ref = 1.f, float aMin = 1e-10f, float topDb = 80.f);
	static void Amplitude2Db(float* spectrInOut, int size,
		float ref = 1.f, float aMin = 1e-5f, float topDb = 80.f);
};