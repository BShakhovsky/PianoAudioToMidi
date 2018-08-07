#include "stdafx.h"
#include "AlignedVector.h"
#include "CqtBasis.h"
#include "CqtBasisData.h"
#include "SparseMatrix.h"
#include "CqtError.h"

using namespace std;
using boost::alignment::is_aligned;
typedef CqtBasis::CQT_WINDOW WIN_FUNC;

CqtBasisData::CqtBasisData(const int rate, const float fMin, const size_t nBins, const int octave,
	const int scale, const int hopLen, const CqtBasis::CQT_WINDOW window)
	: rate_(rate), window_(window),
	Q_(scale / (pow(2.f, 1.f / octave) - 1)),
	freqs_(nBins), lens_(nBins),
	bInd_(0), offset_(0), buff_(nullptr), size_(0)
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

CqtBasisData::~CqtBasisData() {}


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

	assert(filts_.size() == freqs_.size() && freqs_.size() == lens_.size() &&
		"Mistake in CQT-basis array sizes");
#ifdef _DEBUG
	for (const auto& f : filts_) assert(f.size() == nFft_ / 2 && "Wrong CQT-basis filter size");
#endif

	SparsifyRows(sparsity);
}

void CqtBasisData::RowMajorMultiply(const MKL_Complex8* src, MKL_Complex8* dest, const int nDestCols) const
{
	// If there are 99.9% of zeros, then mult time may be milliseconds instead of seconds:
	if (csr_) csr_->RowMajorMultiply(src, dest, nDestCols);
	else // If not, dense multiplication would be quicker:
	{
		complex<float> alpha(1), beta(0);

//		A = (double *)mkl_malloc(m*k * sizeof(double), 64);
//		B = (double *)mkl_malloc(k*n * sizeof(double), 64);
//		C = (double *)mkl_malloc(m*n * sizeof(double), 64);
//
//		m, n, k, alpha, a, lda, b, ldb, beta, c, ldc)
//		cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
//			m, n, k, alpha, A, k, B, n, beta, C, n);

		cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, static_cast<int>(filts_.size()),
			nDestCols, static_cast<int>(filts_.front().size()), &alpha, filtsFlat_.data(),
			static_cast<int>(filts_.front().size()), src, nDestCols, &beta, dest, nDestCols);
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

	assert(is_aligned(freqs_.data(), 64) && "CQT frequencies are not aligned");
}

void CqtBasisData::CalcLengths()
{
	// Fractional lengths of each filter
	const auto unusedIter(transform(freqs_.cbegin(), freqs_.cend(), lens_.begin(),
		[this](float freq) { return Q_ * rate_ / freq; }));
	assert(lens_.front() == *max_element(lens_.cbegin(), lens_.cend()) &&
		"Mistake in CQT-basis filter lengths");
	assert(is_aligned(lens_.data(), 64) && "CQT-basis filter lengths are not aligned");
}


void CqtBasisData::CalcBufferPointers(const size_t binIndex)
{
	bInd_ = binIndex;
	offset_ = (filts_.at(binIndex).size() - static_cast<size_t>(lens_.at(binIndex)) - 1) / 2;
	buff_ = reinterpret_cast<Ipp32fc*>(filts_.at(binIndex).data()) + offset_;
	// TODO:
//	assert(is_aligned(buff_, 64) && "CQT-basis filters are not aligned");
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
	// TODO:
//	assert(is_aligned(filts_.at(bInd_).data()_, 64) && "CQT-basis filters are not aligned");
	fft_->perform(filts_.at(bInd_).data(), filts_.at(bInd_).data(), false);
	filts_.at(bInd_).resize(nFft_ / 2); // Retain only the non-negative frequencies
	// TODO:
//	assert(is_aligned(filts_.at(bInd_).data()_, 64) && "CQT-basis filters are not aligned");
}


void CqtBasisData::SparsifyRows(const float quantile)
{
	auto startTime(dsecnd());

	assert(0 <= quantile and quantile < 1 && "Quantile should be between zero and one");
	if (quantile == 0) return; /* Dense matrix multiplication will be faster
	However, if quantile > 0, it will zero-out 99.9% of values, then sparse mult will make sense
	Especially if num filters = not 88, but one octave (12 * nBins),
	then multiplication time will be milliseconds compared to seconds */

	string errMsg;
	IppSizeL radixSize;
	auto status(ippsSortRadixGetBufferSize_L(static_cast<IppSizeL>(
		filts_.front().size()), ipp32f, &radixSize));
	switch (status)
	{
	case ippStsNoErr:																break;
	case ippStsNullPtrErr:	errMsg = "pointer to radix sort buffer size = null";	break;
	case ippStsSizeErr:		errMsg = "length <= 0";									break;
	case ippStsDataTypeErr: errMsg = "data type not supported";						break;
	default:				errMsg = "unknown error";
	}
	if (not errMsg.empty()) throw CqtError((
		"Could not calculate the buffer size for the radix sort function: "
		+ errMsg + '\n' + ippGetStatusString(status)).c_str());
	AlignedVector<Ipp8u> radixBuff(static_cast<size_t>(radixSize));

//#ifdef _DEBUG
	size_t nNonZeros(0);
//#endif
	for (auto& filt : filts_)
	{
		errMsg.clear();
		AlignedVector<Ipp32f> mags(filt.size());
		status = ippsMagnitude_32fc(reinterpret_cast<Ipp32fc*>(filt.data()), mags.data(),
			static_cast<int>(filt.size()));
		switch (status)
		{
		case ippStsNoErr:															break;
		case ippStsNullPtrErr:	errMsg = "any of the specified pointers is null";	break;
		case ippStsSizeErr:		errMsg = "length <= 0";								break;
		default:				errMsg = "unknown error";
		}
		if (not errMsg.empty()) throw CqtError(("Could not calculate CQT-basis complex filter magnitudes: "
			+ errMsg + '\n' + ippGetStatusString(status)).c_str());

		errMsg.clear();
		auto magsSort(mags);
		status = ippsSortRadixAscend_32f_I_L(magsSort.data(), static_cast<int>(magsSort.size()), radixBuff.data());
		switch (status)
		{
		case ippStsNoErr:																			break;
		case ippStsNullPtrErr:	errMsg = "either source buffer or internal buffer pointer is null";	break;
		case ippStsSizeErr:		errMsg = "length <= 0";												break;
		default:				errMsg = "unknown error";
		}
		if (not errMsg.empty()) throw CqtError(("Could not radix-sort CQT-basis filter magnitudes: "
			+ errMsg + '\n' + ippGetStatusString(status)).c_str());

		errMsg.clear();
		Ipp32f l1Norm;
		status = ippsSum_32f(magsSort.data(), static_cast<int>(magsSort.size()), &l1Norm, ippAlgHintFast);
		switch (status)
		{
		case ippStsNoErr:																			break;
		case ippStsNullPtrErr:	errMsg = "either source buffer or pointer to sum value is null";	break;
		case ippStsSizeErr:		errMsg = "length <= 0";												break;
		default:				errMsg = "unknown error";
		}
		if (not errMsg.empty()) throw CqtError(("Could not sum CQT-basis filter magnitudes: "
			+ errMsg + '\n' + ippGetStatusString(status)).c_str());

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
//#ifdef _DEBUG
			else ++nNonZeros;
//#endif
	}

	// Now, after all calculations are completed, it is convenient time to flatten the array:
	filtsFlat_.assign(filts_.size() * filts_.front().size(), 0);
	vector<complex<float>>::iterator unusedIter;
	for (size_t j(0); j < filts_.size(); ++j) unusedIter = copy(filts_.at(j).cbegin(),
		filts_.at(j).cend(), filtsFlat_.begin() + static_cast<ptrdiff_t>(j * filts_.at(j).size()));

	// TODO:
//	assert(is_aligned(filtsFlat_.data(), 64) && "CQT-basis filters: flat array is not aligned");


	cout << "Zeroing out time = " << dsecnd() - startTime << endl;

	int nDestCols(16'000);
	AlignedVector<MKL_Complex8> src(filts_.front().size() * nDestCols), dst(filts_.size() * nDestCols);
	const int nLoops(10);
	RowMajorMultiply(src.data(), dst.data(), nDestCols);
	startTime = dsecnd();
	for (int i(0); i < nLoops; ++i) RowMajorMultiply(src.data(), dst.data(), nDestCols);
	cout << "Dense multiplication time = " << (dsecnd() - startTime) / nLoops << endl;
	system("Pause");

	startTime = dsecnd();
	csr_ = make_unique<SparseMatrix>(reinterpret_cast<MKL_Complex8*>(filtsFlat_.data()),
		static_cast<int>(filts_.size()), static_cast<int>(filts_.front().size()),
#ifdef _DEBUG
//		nNonZeros
		filtsFlat_.size() // just a little bit quicker not to calculate num non-zeros
#elif defined NDEBUG
		nNonZeros
//		filtsFlat_.size() // just a little bit quicker not to calculate num non-zeros
#else
#	error Not debug, not release, then what is it?
#endif
		);
	cout << "Dense to sparse conversion time = " << dsecnd() - startTime << endl;

	RowMajorMultiply(src.data(), dst.data(), nDestCols);
	startTime = dsecnd();
	for (int i(0); i < nLoops; ++i) RowMajorMultiply(src.data(), dst.data(), nDestCols);
	cout << "Sparse multiplication time = " << (dsecnd() - startTime) / nLoops << endl;
	system("Pause");
}