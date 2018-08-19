#pragma once

class CqtBasis
{
public:
	explicit CqtBasis(int binsPerOctave, float filterScale, NORM_TYPE norm = NORM_TYPE::L1,
		ConstantQ::CQT_WINDOW window = ConstantQ::CQT_WINDOW::HANN);
	~CqtBasis();

	void CalcFrequencies(int sampleRate, float fMin, size_t nBins);
	void CalcLengths(int sampleRate, float fMin, size_t nBins);
	void CalcFilters(int sampleRate, float fMin, size_t nBins, float sparsity, int hopLen = 0);

	void ScaleFilters(float scale);
	void RowMajorMultiply(const MKL_Complex8* source, MKL_Complex8* dest, int nDestColumns) const;
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	float GetQfactor() const { return Q_; }
	const std::vector<float>& GetFrequencies() const { return freqs_; }
	std::vector<float>& GetLengths() { return lens_; } // not const, for inplace sqrt from outside
	size_t GetFftFrameLen() const { return nFft_; }
#pragma warning(pop)
private:
	void SparsifyRows(float quantile);

	const int octave_;
	const float Q_;
	const NORM_TYPE norm_;
	const ConstantQ::CQT_WINDOW window_;

	std::vector<float> freqs_, lens_;

	std::vector<std::vector<std::complex<float>>> filts_;
	AlignedVector<std::complex<float>> filtsFlat_;
	size_t nFft_;

	std::unique_ptr<class SparseMatrix> csr_;

	CqtBasis(const CqtBasis&) = delete;
	CqtBasis operator=(const CqtBasis&) = delete;
};