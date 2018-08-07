#pragma once

class CqtBasis
{
public:
	enum class CQT_WINDOW { RECT, HANN, HAMMING };

	explicit CqtBasis(int sampleRate, float fMin, size_t nBins, int binsPerOctave,
		int filterScale, int hopLen = 0, CQT_WINDOW window = CQT_WINDOW::HANN);
	~CqtBasis();

	void Calculate(float sparsity) const;

	const std::vector<std::vector<std::complex<float>>>& GetCqtFilters() const;
	const std::vector<std::complex<float>>& GetFlatFilers() const;
private:
	const std::unique_ptr<struct CqtBasisData> data_;

	CqtBasis(const CqtBasis&) = delete;
	CqtBasis operator=(const CqtBasis&) = delete;
};