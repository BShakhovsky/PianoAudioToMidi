#include "stdafx.h"
#include "ConstantQ.h"
#include "CqtBasis.h"
#include "SparseMatrix.h"
#include "AlignedVector.h"
#include "CqtError.h"

using namespace std;

#ifdef _DEBUG
void CheckIPPresult(IppStatus status)
{
	string errMsg;
	switch (status)
	{
	case ippStsNoErr:																break;
	case ippStsNullPtrErr:		errMsg = "Null pointer: ";							break;
	case ippStsSizeErr:			errMsg = "Length <= 0 (or <= 3): ";					break;
	case ippStsDivByZeroErr:	errMsg = "Division by value < min float number: ";	break;
	case ippStsDataTypeErr:		errMsg = "Data type not supported: ";				break;
	default:					errMsg = "Unknown error: ";
	}
	if (not errMsg.empty()) throw CqtError((errMsg + ippGetStatusString(status)).c_str());
}
#	define CHECK_IPP_RESULT(STATUS) CheckIPPresult(STATUS)
#elif defined NDEBUG
#	define CHECK_IPP_RESULT(STATUS) STATUS
#else
#	error Not debug, not release, then what is it?
#endif

CqtBasis::CqtBasis(const size_t octave, const float scale, const ConstantQ::CQT_WINDOW window)
	: octave_(octave), Q_(scale / (pow(2.f, 1.f / octave_) - 1)), window_(window)
{
	// Larger filter scale --> larger Q-factor --> larger windows -->
	// --> increased frequency resolution at the cost of time resolution
	assert(scale > 0 && "Filter scale must be positive");
}

//#pragma warning(push)
//#pragma warning(disable:4710) // Function not inlined
CqtBasis::~CqtBasis() {}
//#pragma warning(pop)

void CqtBasis::CalcFrequencies(const size_t rate, const float fMin, const size_t nBins)
{
	assert(fMin > 0 && "Minimum frequency must be positive");

	// Equivalent noise bandwidth (int FFT bins) of a window function:
	constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	freqs_.resize(nBins);
	for (size_t i(0); i < nBins; ++i) freqs_.at(i) = fMin * pow(2.f, static_cast<float>(i) / octave_);
	if (freqs_.back() * (1 + 0.5 * WIN_BAND_WIDTH[static_cast<int>(window_)] / Q_) > rate / 2.)
		throw CqtError("Filter pass-band lies beyond Nyquist");
}

void CqtBasis::Calculate(const size_t rate, const float fMin,
	const size_t nBins, const float sparsity, const int hopLen)
{
	using juce::dsp::FFT;

	CalcFrequencies(rate, fMin, nBins);

	// Fractional lengths of each filter:
	vector<float> lens(nBins);
	const auto unusedIter(transform(freqs_.cbegin(), freqs_.cend(), lens.begin(),
		[this, rate](float freq) { return Q_ * rate / freq; }));
	assert(lens.front() == *max_element(lens.cbegin(), lens.cend()) &&
		"Mistake in CQT-basis filter lengths");

	// All filters will be center-padded up to the nearest integral power of 2:
	auto nFft(static_cast<size_t>(pow(2.f, ceil(log2(lens.front())))));
	filts_.assign(nBins, vector<complex<float>>(nFft, 0if));

	// Eventually constant-Q filter basis will be transformed from time- to the frequency-domain
	// and re-normalized to fft-window length:
	if (hopLen) nFft = max(nFft, static_cast<size_t>(pow(2, 1 + ceil(log2(hopLen)))));
	FFT fft(static_cast<int>(log2(nFft)));
	assert(nFft == static_cast<size_t>(fft.getSize()) &&
		"Mistake in rounding to the nearest integral power of 2");

	for (size_t i(0); i < filts_.size(); ++i)
	{
		const auto offset((filts_.at(i).size() - static_cast<size_t>(lens.at(i)) - 1) / 2);

		/* Time-domain filter bank described by McVicar, Matthew
		"A machine learning approach to automatic chord extraction."
		Dissertation, University of Bristol. 2013.*/
		for (size_t j(0); j <= lens.at(i); ++j) // length will be ceil(cqLen)
			filts_.at(i).at(j + offset) = exp(floor(j - lens.at(i) / 2) / rate
				* 2if * static_cast<float>(M_PI) * freqs_.at(i));

		const auto buff(reinterpret_cast<Ipp32fc*>(filts_.at(i).data()) + offset);
		// TODO:
//		assert(is_aligned(buff, 64) && "CQT-basis filters are not aligned");
		const auto size(static_cast<int>(ceil(lens.at(i))));

		switch (window_)
		{
		case ConstantQ::CQT_WINDOW::RECT:							break;
		case ConstantQ::CQT_WINDOW::HANN:		CHECK_IPP_RESULT(ippsWinHann_32fc_I(
			// +1 if even to compensate for non-symmetry:
			buff, size + 1 - static_cast<int>(lens.at(i)) % 2));	break;
		case ConstantQ::CQT_WINDOW::HAMMING:	CHECK_IPP_RESULT(ippsWinHamming_32fc_I(
			buff, size + 1 - static_cast<int>(lens.at(i)) % 2));	break;
		default: assert(!"Not all CQT windowing functions checked");
		}

		Ipp64f norm;
		CHECK_IPP_RESULT(ippsNorm_L1_32fc64f(buff, size, &norm));
		CHECK_IPP_RESULT(ippsMulC_32fc_I({ lens.at(i) / nFft /
			static_cast<Ipp32f>(norm), 0 }, buff, size));

		// TODO:
//		assert(is_aligned(filts_.at(i).data(), 64) && "CQT-basis filters are not aligned");
		fft.perform(filts_.at(i).data(), filts_.at(i).data(), false);
		filts_.at(i).resize(nFft / 2); // Retain only the non-negative frequencies
		// TODO:
//		assert(is_aligned(filts_.at(i).data(), 64) && "CQT-basis filters are not aligned");
	}

	assert(filts_.size() == freqs_.size() && freqs_.size() == lens.size() &&
		"Mistake in CQT-basis array sizes");
#ifdef _DEBUG
	for (const auto& f : filts_) assert(f.size() == nFft / 2 && "Wrong CQT-basis filter size");
#endif

	SparsifyRows(sparsity);
}

void CqtBasis::RowMajorMultiply(const MKL_Complex8* src,
	MKL_Complex8* dest, const int nDestCols) const
{
	// If there are 99.9% of zeros, then mult time may be milliseconds instead of seconds:
	if (csr_) csr_->RowMajorMultiply(src, dest, nDestCols);
	else // If not, dense multiplication would be quicker:
	{
		complex<float> alpha(1), beta(0);
		cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, static_cast<int>(filts_.size()),
			nDestCols, static_cast<int>(filts_.front().size()), &alpha, filtsFlat_.data(),
			static_cast<int>(filts_.front().size()), src, nDestCols, &beta, dest, nDestCols);
	}
}

void CqtBasis::SparsifyRows(const float quantile)
{
	assert(0 <= quantile and quantile < 1 && "Quantile should be between zero and one");
	size_t nNonZeros(0);
	if (quantile > 0)
	{
		/* Quantile will zero-out 99.9% of values, then sparse mult will make sense
		Especially if num filters = not 88, but one octave (12 * nBins),
		then multiplication time will be milliseconds compared to seconds.

		Otherwise, dense matrix multiplication will be faster*/

		IppSizeL radixSize;
		CHECK_IPP_RESULT(ippsSortRadixGetBufferSize_L(static_cast<IppSizeL>(
			filts_.front().size()), ipp32f, &radixSize));
		AlignedVector<Ipp8u> radixBuff(static_cast<size_t>(radixSize));

		for (auto& filt : filts_)
		{
			AlignedVector<Ipp32f> mags(filt.size());
			CHECK_IPP_RESULT(ippsMagnitude_32fc(reinterpret_cast<Ipp32fc*>(filt.data()),
				mags.data(), static_cast<int>(filt.size())));

			auto magsSort(mags);
			CHECK_IPP_RESULT(ippsSortRadixAscend_32f_I_L(magsSort.data(),
				static_cast<int>(magsSort.size()), radixBuff.data()));

			Ipp32f l1Norm;
			CHECK_IPP_RESULT(ippsSum_32f(magsSort.data(),
				static_cast<int>(magsSort.size()), &l1Norm, ippAlgHintFast));

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
	}

	// Now, after all calculations are completed, it is convenient time to flatten the array:
	filtsFlat_.assign(filts_.size() * filts_.front().size(), 0);
	vector<complex<float>>::iterator unusedIter;
	for (size_t j(0); j < filts_.size(); ++j) unusedIter = copy(filts_.at(j).cbegin(),
		filts_.at(j).cend(), filtsFlat_.begin() + static_cast<ptrdiff_t>(j * filts_.at(j).size()));

	// TODO:
//	assert(is_aligned(filtsFlat_.data(), 64) && "CQT-basis filters: flat array is not aligned");

	if (quantile > 0) csr_ = make_unique<SparseMatrix>(reinterpret_cast<MKL_Complex8*>(filtsFlat_.data()),
		static_cast<int>(filts_.size()), static_cast<int>(filts_.front().size()), nNonZeros);
}