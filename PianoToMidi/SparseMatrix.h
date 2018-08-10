#pragma once

class SparseMatrix
{
public:
	explicit SparseMatrix(MKL_Complex8* dense, int nRows, int nCols, size_t nNonZeros);
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	~SparseMatrix() { Destroy(); }
#pragma warning(pop)

	void Scale(Ipp32f scale);
	void RowMajorMultiply(const MKL_Complex8* source, MKL_Complex8* dest, int nDestColumns) const;
private:
	void Create();
	void Destroy();

	const int nCols_;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#elif not defined _WIN32
#	error It should be either 32- or 64-bit Windows
#endif
	AlignedVector<MKL_Complex8> Aval_;
	AlignedVector<int> Acol_, Arow_;

	sparse_matrix_t csr_;

	SparseMatrix operator=(const SparseMatrix&) = delete;
};