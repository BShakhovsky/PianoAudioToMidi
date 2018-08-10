#include "stdafx.h"

#include "AlignedVector.h"
#include "SparseMatrix.h"
#include "IntelCheckStatus.h"

SparseMatrix::SparseMatrix(MKL_Complex8* dense, const int nRows, const int nCols, const size_t nNonZeros)
	: nCols_(nCols), Aval_(nNonZeros), Acol_(nNonZeros),
	Arow_(nRows + 1ull), // +1 is because 3-array variation
	csr_(nullptr)
{
#ifdef _DEBUG
	using boost::alignment::is_aligned;
#endif

	const int job[] = { 0, 0, 0, 2, static_cast<int>(nNonZeros), 1 };
	int info;
#pragma warning(suppress:4996) // was declared deprecated
	mkl_cdnscsr(job, &nRows, &nCols, dense, &nCols,
		Aval_.data(), Acol_.data(), Arow_.data(), &info);
	assert(not info && "Could not convert CQT-basis filters dense matrix to sparse CSR format" &&
		"The routine is interrupted processing the info-th row, because there is no space" &&
		"in the value array and column-indices array according to the nNonZeros");

	Create();

	assert(is_aligned(Aval_.data(), 64) && "Sparse matrix values are not aligned");
	assert(is_aligned(Acol_.data(), 64) && "Sparse matrix column indices are not aligned");
	assert(is_aligned(Arow_.data(), 64) && "Sparse matrix row indices are not aligned");
}

void SparseMatrix::Create()
{
	assert(not csr_ and "Sparse matrix already created");

	CHECK_MKL_RESULT(mkl_sparse_c_create_csr(&csr_, SPARSE_INDEX_BASE_ZERO,
		static_cast<int>(Arow_.size() - 1), nCols_,
		Arow_.data(), Arow_.data() + 1, Acol_.data(), Aval_.data()));
}

void SparseMatrix::Destroy()
{
	// Aval_, Acol_, Arow_ will be deallocated automatically since they are vectors,
	// so no need to call mkl_free, plus, we may still need them if it is scaling operation

	if (not csr_) return;
	CHECK_MKL_RESULT(mkl_sparse_destroy(csr_));
	csr_ = nullptr;
}

void SparseMatrix::Scale(const Ipp32f scale)
{
	assert(csr_ and "Sparse matrix has not been created yet, nothing to scale");

	CHECK_IPP_RESULT(ippsMulC_32fc_I({ scale, 0 },
		reinterpret_cast<Ipp32fc*>(Aval_.data()), static_cast<int>(Aval_.size())));

	Destroy();
	Create();
}

void SparseMatrix::RowMajorMultiply(const MKL_Complex8* src,
	MKL_Complex8* res, const int nDestCols) const
{
	assert(csr_ and "Sparse matrix not created");

	matrix_descr descr{ SPARSE_MATRIX_TYPE_GENERAL, SPARSE_FILL_MODE_FULL };
	CHECK_MKL_RESULT(mkl_sparse_c_mm(SPARSE_OPERATION_NON_TRANSPOSE, { 1, 0 }, csr_, descr,
		SPARSE_LAYOUT_ROW_MAJOR, src, nDestCols, nDestCols, { 0, 0 }, res, nDestCols));
}