#pragma once

class CqtBasis
{
public:
	explicit CqtBasis(size_t binsPerOctave, float filterScale,
		ConstantQ::CQT_WINDOW window = ConstantQ::CQT_WINDOW::HANN);
	~CqtBasis();

	void CalcFrequencies(size_t sampleRate, float fMin, size_t nBins);
	void Calculate(size_t sampleRate, float fMin, size_t nBins, float sparsity, int hopLen = 0);
	void RowMajorMultiply(const MKL_Complex8* source, MKL_Complex8* dest, int nDestColumns) const;

	float GetQfactor() const { return Q_; }
	const std::vector<std::vector<std::complex<float>>>& GetCqtFilters() const { return filts_; }
	const std::vector<std::complex<float>>& GetFlatFilers() const { return filtsFlat_; }
private:
	void SparsifyRows(float quantile);

	const size_t octave_;
	const float Q_;
	const ConstantQ::CQT_WINDOW window_;

	std::vector<float> freqs_;

	// TODO: AlignedVector:
	std::vector<std::vector<std::complex<float>>> filts_;
	std::vector<std::complex<float>> filtsFlat_;

	std::unique_ptr<class SparseMatrix> csr_;

	CqtBasis(const CqtBasis&) = delete;
	CqtBasis operator=(const CqtBasis&) = delete;
};