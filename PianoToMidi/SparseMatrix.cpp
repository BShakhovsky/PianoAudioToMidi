#include "stdafx.h"
#include "SparseMatrix.h"
#include "AlignedVector.h"

#ifdef _DEBUG
void CheckMKLresult(sparse_status_t status)
{
	switch (status)
	{
	case SPARSE_STATUS_SUCCESS:										break;
	case SPARSE_STATUS_NOT_INITIALIZED:		assert(not
		"The routine encountered an empty handle or matrix array");	break;
	case SPARSE_STATUS_ALLOC_FAILED:		assert(not
		"Internal memory allocation failed");						break;
	case SPARSE_STATUS_INVALID_VALUE:		assert(not
		"The input parameters contain an invalid value");			break;
	case SPARSE_STATUS_EXECUTION_FAILED:	assert(not
		"Execution failed");										break;
	case SPARSE_STATUS_INTERNAL_ERROR:		assert(not
		"An error in algorithm implementation occurred");			break;
	case SPARSE_STATUS_NOT_SUPPORTED:		assert(not
		"The requested operation is not supported");				break;
	default:								assert(not
		"Unknown error");
	}
}
#	define CHECK_MKL_RESULT(STATUS) CheckMKLresult(STATUS)
#elif defined NDEBUG
#	define CHECK_MKL_RESULT(STATUS) STATUS
#else
#	error Not debug, not release, then what is it?
#endif

SparseMatrix::SparseMatrix(MKL_Complex8* dense, const int nRows, const int nCols, const size_t nNonZeros)
{
	using boost::alignment::is_aligned;

	AlignedVector<MKL_Complex8> A_val(nNonZeros);
	AlignedVector<int> A_col(nNonZeros), A_row(nRows + 1ull); // +1 is because 3-array variation

	const int job[] = { 0, 0, 0, 2, static_cast<int>(nNonZeros), 1 };
	int info;
#pragma warning(suppress:4996) // was declared deprecated
	mkl_cdnscsr(job, &nRows, &nCols, dense, &nCols,
		A_val.data(), A_col.data(), A_row.data(), &info);
	assert(not info && "Could not convert CQT-basis filters dense matrix to sparse CSR format" &&
		"The routine is interrupted processing the info-th row, because there is no space" &&
		"in the value array and column-indices array according to the nNonZeros");

	CHECK_MKL_RESULT(mkl_sparse_c_create_csr(&csr_, SPARSE_INDEX_BASE_ZERO, nRows, nCols,
		A_row.data(), A_row.data() + 1, A_col.data(), A_val.data()));

	assert(is_aligned(A_val.data(), 64) && "Sparse matrix values are not aligned");
	assert(is_aligned(A_col.data(), 64) && "Sparse matrix column indices are not aligned");
	assert(is_aligned(A_row.data(), 64) && "Sparse matrix row indices are not aligned");

	// Now A_val, A_col, A_row will be deallocated automatically, no need to call mkl_free
}

#pragma warning(push)
#pragma warning(disable:4711) // Selected for automatic inline expansion
SparseMatrix::~SparseMatrix() { if (csr_) CHECK_MKL_RESULT(mkl_sparse_destroy(csr_)); }
#pragma warning(pop)

void SparseMatrix::RowMajorMultiply(const MKL_Complex8* src,
	MKL_Complex8* res, const int nDestCols) const
{
	matrix_descr descr{ SPARSE_MATRIX_TYPE_GENERAL, SPARSE_FILL_MODE_FULL };
	CHECK_MKL_RESULT(mkl_sparse_c_mm(SPARSE_OPERATION_NON_TRANSPOSE, { 1, 0 }, csr_, descr,
		SPARSE_LAYOUT_ROW_MAJOR, src, nDestCols, nDestCols, { 0, 0 }, res, nDestCols));
}