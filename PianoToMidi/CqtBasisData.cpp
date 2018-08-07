#include "stdafx.h"
#include "AlignedVector.h"
#include "CqtBasis.h"
#include "CqtBasisData.h"
#include "CqtError.h"

using namespace std;
typedef CqtBasis::CQT_WINDOW WIN_FUNC;

CqtBasisData::CqtBasisData(const int rate, const float fMin, const size_t nBins, const int octave,
	const int scale, const int hopLen, const CqtBasis::CQT_WINDOW window)
	: rate_(rate), window_(window),
	Q_(scale / (pow(2.f, 1.f / octave) - 1)),
	freqs_(nBins), lens_(nBins),
	bInd_(0), offset_(0), buff_(nullptr), size_(0),
	isSparse_(false)
{
	using juce::dsp::FFT;

	assert(nBins > 0 && "Number of bins must be positive");
	assert(scale > 0 && "Filter scale must be positive");

	CalcFrequencies(fMin, octave);
	CalcLengths();

	// All filters will be center-padded up to the nearest integral power of 2:
	nFft_ = static_cast<size_t>(pow(2.f, ceil(log2(lens_.front()))));
	filts_.assign(nBins, vector<complex<float>>(nFft_, 0if));

	// Eventually constant-Q filter basis will be transformed from time- to the frequency-domain
	// and re-normalized to fft-window length:
	if (hopLen) nFft_ = max(nFft_, static_cast<size_t>(pow(2, 1 + ceil(log2(hopLen)))));
	fft_ = make_unique<FFT>(static_cast<int>(log2(nFft_)));
	assert(nFft_ == static_cast<size_t>(fft_->getSize()) &&
		"Mistake in rounding to the nearest integral power of 2");
}

void CqtBasisData::Calculate(const float sparsity)
{
	for (size_t i(0); i < filts_.size(); ++i)
	{
		CalcBufferPointers(i);
		CalcTimeDomainFilter();
		MultiplyWindow();
		Normalize();
		FilterFft();
	}

	SparsifyRows(sparsity);
}

void CqtBasisData::Multiply() const
{
	if (not isSparse_)
	{
//		cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, alpha, A, k, B, n, beta, C, n);
	}
}


void CqtBasisData::CalcFrequencies(const float fMin, const int octave)
{
	assert(fMin > 0 && "Minimum frequency must be positive");
	assert(octave > 0 && "Bins per octave must be positive");

	// Equivalent noise bandwidth (int FFT bins) of a window function:
	constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	for (size_t i(0); i < freqs_.size(); ++i)
		freqs_.at(i) = fMin * pow(2.f, static_cast<float>(i) / octave);
	assert(freqs_.back() * (1 + 0.5 * WIN_BAND_WIDTH[static_cast<int>(window_)] / Q_) <= rate_ / 2. &&
		"Filter pass-band lies beyond Nyquist");
}

void CqtBasisData::CalcLengths()
{
	// Fractional lengths of each filter
	const auto unusedIter(transform(freqs_.cbegin(), freqs_.cend(), lens_.begin(),
		[this](float freq) { return Q_ * rate_ / freq; }));
	assert(lens_.front() == *max_element(lens_.cbegin(), lens_.cend()));
}


void CqtBasisData::CalcBufferPointers(const size_t binIndex)
{
	bInd_ = binIndex;
	offset_ = (filts_.at(binIndex).size() - static_cast<size_t>(lens_.at(binIndex)) - 1) / 2;
	buff_ = reinterpret_cast<Ipp32fc*>(filts_.at(binIndex).data()) + offset_;
	size_ = static_cast<int>(ceil(lens_.at(binIndex)));
}

void CqtBasisData::CalcTimeDomainFilter()
{
	/* Filter bank described by McVicar, Matthew
	"A machine learning approach to automatic chord extraction."
	Dissertation, University of Bristol. 2013.*/

	for (size_t j(0); j <= lens_.at(bInd_); ++j) // length will be ceil(cqLen)
		filts_.at(bInd_).at(j + offset_) = exp(floor(j - lens_.at(bInd_) / 2) / rate_
			* 2if * static_cast<float>(M_PI) * freqs_.at(bInd_));
}

void CqtBasisData::MultiplyWindow() const
{
	string errMsg;
	IppStatus status(ippStsErr);
	switch (window_)
	{
	case WIN_FUNC::RECT:	status = ippStsNoErr;					break;
	case WIN_FUNC::HANN:	status = ippsWinHann_32fc_I(
		// +1 if even to compensate for non-symmetry:
		buff_, size_ + 1 - static_cast<int>(lens_.at(bInd_)) % 2);	break;
	case WIN_FUNC::HAMMING:	status = ippsWinHamming_32fc_I(
		buff_, size_ + 1 - static_cast<int>(lens_.at(bInd_)) % 2);	break;
	default: assert(!"Not all CQT windowing functions checked");
	}
	switch (status)
	{
	case ippStsNoErr:											break;
	case ippStsNullPtrErr:	errMsg = "buffer pointer is null";	break;
	case ippStsSizeErr:		errMsg = "length < 3";				break;
	default:				errMsg = "unknown error";
	}
	if (not errMsg.empty()) throw CqtError(("Could not multiply by Windowing Function: "
		+ errMsg + '\n' + ippGetStatusString(status)).c_str());
}

void CqtBasisData::Normalize() const
{
	string errMsg;
	Ipp64f norm;
	auto status(ippsNorm_L1_32fc64f(buff_, size_, &norm));
	switch (status)
	{
	case ippStsNoErr:																	break;
	case ippStsNullPtrErr:	errMsg = "either buffer or norm factor pointer is null";	break;
	case ippStsSizeErr:		errMsg = "length <= 0";										break;
	default:				errMsg = "unknown error";
	}
	if (not errMsg.empty()) throw CqtError(("Could not calculate L1-norm: "
		+ errMsg + '\n' + ippGetStatusString(status)).c_str());

	errMsg.clear();
	status = ippsMulC_32fc_I({ lens_.at(bInd_) / nFft_ / static_cast<Ipp32f>(norm), 0 }, buff_, size_);
	switch (status)
	{
	case ippStsNoErr:																break;
	case ippStsNullPtrErr:		errMsg = "buffer pointer is null";					break;
	case ippStsSizeErr:			errMsg = "length <= 0";								break;
//	case ippStsDivByZeroErr:	errMsg = "division by value < min float number";	break;
	default:					errMsg = "unknown error";
	}
	if (not errMsg.empty()) throw CqtError((
		"Could not re-normalize bases with respect to the FFT window length: "
		+ errMsg + '\n' + ippGetStatusString(status)).c_str());
}

void CqtBasisData::FilterFft()
{
	fft_->perform(filts_.at(bInd_).data(), filts_.at(bInd_).data(), false);
	filts_.at(bInd_).resize(nFft_ / 2); // Retain only the non-negative frequencies
}


void CqtBasisData::SparsifyRows(const float quantile)
{
	assert(0 <= quantile and quantile < 1 && "Quantile should be between zero and one");
	isSparse_ = quantile > 0;
	if (not isSparse_) return; // Dense matrix multiplication will be faster
	// However, if quantile > 0, it will zero-out 99.9% of values, then sparse mult will make sense

	IppSizeL radixSize;
	ippsSortRadixGetBufferSize_L(static_cast<IppSizeL>(filts_.front().size()), ipp32f, &radixSize);
	AlignedVector<Ipp8u> radixBuff(static_cast<size_t>(radixSize));

	int nNonZeros(0);
	for (auto& filt : filts_)
	{
		AlignedVector<Ipp32f> mags(filt.size());
		ippsMagnitude_32fc(reinterpret_cast<Ipp32fc*>(filt.data()), mags.data(),
			static_cast<int>(filt.size()));
		auto magsSort(mags);
		ippsSortRadixAscend_32f_I_L(magsSort.data(), static_cast<int>(magsSort.size()), radixBuff.data());
		Ipp32f l1Norm;
		ippsSum_32f(magsSort.data(), static_cast<int>(magsSort.size()), &l1Norm, ippAlgHintFast);

		auto threshold(quantile * l1Norm);
		for (size_t i(0); i < magsSort.size(); ++i)
		{
			l1Norm -= magsSort.at(magsSort.size() - i - 1);
			if (l1Norm < threshold)
			{
				threshold = magsSort.at(magsSort.size() - i - 1);
				break;
			}
		}
		for (size_t i(0); i < filt.size(); ++i)
			if (mags.at(i) < threshold) filt.at(i) = 0;
			else ++nNonZeros;
	}

	cout << nNonZeros << endl;

	/*
	const auto A_nnz = m * k, A_rownum = m, A_colnum = k;
	MKL_INT info = 0; // If info = 0, execution of mkl_zdnscsr was successful.

	double* A_val = (double*)mkl_malloc(A_nnz * sizeof(double), ALIGN);
	MKL_INT *A_col = (MKL_INT *)mkl_malloc(A_nnz * sizeof(MKL_INT), ALIGN);
	MKL_INT *A_row = (MKL_INT *)mkl_malloc((A_rownum + 1) * sizeof(MKL_INT), ALIGN); // +1 is because we are using 3-array variation

#pragma warning(suppress:4996) // was declared deprecated
	mkl_ddnscsr(job, &A_rownum, &A_colnum, A, &A_colnum, A_val, A_col, A_row, &info);

	sparse_matrix_t csrA(nullptr);
	mkl_sparse_d_create_csr(&csrA, SPARSE_INDEX_BASE_ZERO, A_rownum, A_colnum, A_row, A_row + 1, A_col, A_val);

	matrix_descr descr{ SPARSE_MATRIX_TYPE_GENERAL, SPARSE_FILL_MODE_FULL };
	mkl_sparse_d_mm(SPARSE_OPERATION_NON_TRANSPOSE, 1, csrA, descr,
		SPARSE_LAYOUT_ROW_MAJOR, B, n, n, 0, C, n);

	//Release matrix handle and deallocate arrays for which we allocate memory ourselves.
	if (mkl_sparse_destroy(csrA) != SPARSE_STATUS_SUCCESS) status = 3;

	//Deallocate arrays for which we allocate memory ourselves.
	mkl_free(A_val); mkl_free(A_col); mkl_free(A_row);

	printf("\n Deallocating memory \n\n");
	mkl_free(A);
	mkl_free(B);
	mkl_free(C);

	const int job[] = { 0, 0, 0, 2, A_nnz, 1 };
	const int m(static_cast<int>(filts_.size())), n(static_cast<int>(filts_.front().size())),
		lda(max(1, n));
	mkl_cdnscsr(job, &m, &n, filts_.data(), &lda, MKL_Complex8* Acsr, int * AJ, int *Al, int *info);
	*/
}