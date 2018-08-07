#pragma once

class SparseMatrix
{
public:
	explicit SparseMatrix(MKL_Complex8* dense, int nRows, int nCols, size_t nNonZeros);
	~SparseMatrix();

	void RowMajorMultiply(const MKL_Complex8* source, MKL_Complex8* dest, int nDestColumns) const;
private:
	sparse_matrix_t csr_;
};