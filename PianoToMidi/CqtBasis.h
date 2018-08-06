#pragma once

class CqtBasis
{
public:
	enum class CQT_WINDOW { RECT, HANN, HAMMING };

	explicit CqtBasis(int sampleRate, float fMin, size_t nBins = 88, int binsPerOctave = 12,
		int filterScale = 1, int hopLen = 0, CQT_WINDOW window = CQT_WINDOW::HANN);
	~CqtBasis();

	void Calculate(float sparsity = .01f) const;

	const std::vector<std::vector<std::complex<float>>>& GetCqtFilters() const;
private:
	const std::unique_ptr<struct CqtBasisData> data_;

	CqtBasis(const CqtBasis&) = delete;
	CqtBasis operator=(const CqtBasis&) = delete;
};