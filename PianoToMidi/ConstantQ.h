#pragma once

class ConstantQ
{
public:
	// norm : { inf, -inf, 0, float > 0 } Type of norm to use for basis function normalization.

	enum class CQT_WINDOW { RECT, HANN, HAMMING };

	explicit ConstantQ(const std::vector<float>& rawAudio, int sampleRate = 22'050,
		int hopLength = 512, float fMin = 27.5f, size_t nBins = 88, int binsPerOctave = 12,
		float filterScale = 1, float norm = 1, float sparsity = .01f,
		CQT_WINDOW windowFunc = CQT_WINDOW::HANN, bool toScale = true, bool isPadReflect = true);
	~ConstantQ();
};