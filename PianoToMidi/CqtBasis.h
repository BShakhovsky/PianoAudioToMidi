#pragma once

class CqtBasis
{
	enum class CQT_WINDOW { RECT, HANN, HAMMING };
public:
	explicit CqtBasis(int sampleRate, float fMin, size_t nBins = 88, int binsPerOctave = 12,
		int filterScale = 1, float sparsity = .01f, int hopLen = 0, CQT_WINDOW window = CQT_WINDOW::HANN);

	const std::vector<std::vector<std::complex<float>>>& GetCqtFilters() const { return filts_; }
private:
	std::vector<std::vector<std::complex<float>>> filts_;
};