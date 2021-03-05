#include "stdafx.h"

#include "AlignedVector.h"
#include "EnumFuncs.h"
#include "ConstantQ.h"
#include "CqtBasis.h"

#include "SparseMatrix.h"
#include "IntelCheckStatus.h"
#include "CqtError.h"

using namespace std;
#ifdef _DEBUG
using boost::alignment::is_aligned;
#endif

CqtBasis::CqtBasis(const int octave, const float scale,
	const NORM_TYPE norm, const ConstantQ::CQT_WINDOW window)
	: octave_(octave), Q_(scale / (pow(2.f, Divide(1, octave_)) - 1)),
	NormFunc_(GetNormFuncComplex(norm)), window_(window),
	nFft_(0)
{
	assert(octave > 0 && "Bins per octave must be positive");
	// Larger filter scale --> larger Q-factor --> larger windows -->
	// --> increased frequency resolution at the cost of time resolution
	assert(scale > 0 && "Filter scale must be positive");
}

CqtBasis::~CqtBasis() {}


void CqtBasis::CalcFrequencies(const int rate, const float fMin, const size_t nBins)
{
	assert(rate > 0 && "Sample rate must be positive");
	assert(fMin > 0 && "Minimum frequency must be positive");
	assert(nBins > 0 && "Number of bins must be positive");

	freqs_.resize(nBins);
	for (size_t i(0); i < nBins; ++i) freqs_.at(i) = fMin * pow(2.f, Divide(i, octave_));
	if (freqs_.back() * (1 + 0.5 * ConstantQ::WIN_BAND_WIDTH[static_cast<int>(window_)] / Q_) > rate / 2.)
		throw CqtError("Filter pass-band lies beyond Nyquist");
}

void CqtBasis::CalcLengths(const int rate, const float fMin, const size_t nBins)
{
	using placeholders::_1;

	CalcFrequencies(rate, fMin, nBins);

	// Fractional lengths of each filter:
	lens_.resize(nBins);
	const auto unusedIter(transform(freqs_.cbegin(), freqs_.cend(), lens_.begin(), bind(divides<float>(), Multiply(Q_, rate), _1)));
	assert(lens_.front() == *max_element(lens_.cbegin(), lens_.cend()) &&
		"Mistake in CQT-basis filter lengths");
}

void CqtBasis::CalcFilters(const int rate, const float fMin,
	const size_t nBins, const float sparsity, const int hopLen)
{
	using juce::dsp::FFT;

	CalcLengths(rate, fMin, nBins);

	// All filters will be center-padded up to the nearest integral power of 2:
	nFft_ = static_cast<size_t>(pow(2.f, ceil(log2(lens_.front()))));
	filts_.assign(nBins, vector<complex<float>>(nFft_, 0if));

	// Eventually constant-Q filter basis will be transformed from time- to the frequency-domain
	// and re-normalized to fft-window length:
	if (hopLen)
	{
		assert(hopLen > 0 && "Hop length must be positive");
		nFft_ = max(nFft_, static_cast<size_t>(pow(2, 1 + ceil(log2(hopLen)))));
	}
	FFT fft(static_cast<int>(log2(nFft_)));
	assert(nFft_ == static_cast<size_t>(fft.getSize()) &&
		"Mistake in rounding to the nearest integral power of 2");

	IppStatus (*WinFunc)(Ipp32fc* srcDst, int len);
	switch (window_)
	{
	case ConstantQ::CQT_WINDOW::RECT:
		WinFunc = [](Ipp32fc*, int) { return ippStsNoErr; };										break;
	case ConstantQ::CQT_WINDOW::HANN:
		WinFunc = [](Ipp32fc* srcDst, int len) { return ippsWinHann_32fc_I		(srcDst, len); };	break;
	case ConstantQ::CQT_WINDOW::HAMMING:
		WinFunc = [](Ipp32fc* srcDst, int len) { return ippsWinHamming_32fc_I	(srcDst, len); };	break;
	default: assert(!"Not all CQT windowing functions checked"); WinFunc = nullptr;
	}

	for (size_t i(0); i < filts_.size(); ++i)
	{
		const auto offset((filts_.at(i).size() - static_cast<size_t>(lens_.at(i)) - 1) / 2);

		/* Time-domain filter bank described by McVicar, Matthew
		"A machine learning approach to automatic chord extraction."
		Dissertation, University of Bristol. 2013.*/
		for (size_t j(0); static_cast<float>(j) <= lens_.at(i); ++j) // length will be ceil(cqLen)
			filts_.at(i).at(j + offset) = exp(Divide(floor(static_cast<float>(j) - lens_.at(i) / 2), rate)
				* 2if * Multiply(M_PI, freqs_.at(i)));

		const auto buff(reinterpret_cast<Ipp32fc*>(filts_.at(i).data()) + offset);
		const auto size(static_cast<int>(ceil(lens_.at(i))));

		// +1 if even to compensate for non-symmetry:
		if (WinFunc) CHECK_IPP_RESULT(WinFunc(buff, size + 1 - static_cast<int>(lens_.at(i)) % 2));
		
		Ipp32f norm32(0);
		if (NormFunc_) CHECK_IPP_RESULT(NormFunc_(buff, size, &norm32));
		assert(norm32 && "Norm factor not calculated");
		CHECK_IPP_RESULT(ippsMulC_32fc_I({ Divide(lens_.at(i), nFft_) / norm32, 0 }, buff, size));

		fft.perform(filts_.at(i).data(), filts_.at(i).data(), false);
		filts_.at(i).resize(nFft_ / 2 + 1); // Retain only the non-negative frequencies
	}

	assert(filts_.size() == freqs_.size() && freqs_.size() == lens_.size() &&
		"Mistake in CQT-basis array sizes");
#ifdef _DEBUG
	for (const auto& f : filts_) assert(f.size() == nFft_ / 2 + 1 && "Wrong CQT-basis filter size");
#endif

	SparsifyRows(sparsity);
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

	// Now, after all calculations are completed,
	// it is convenient time to flatten the array and get rid of 2D-buffer:
	filtsFlat_.assign(filts_.size() * filts_.front().size(), 0);
	AlignedVector<complex<float>>::iterator unusedIter;
	for (size_t j(0); j < filts_.size(); ++j) unusedIter = copy(filts_.at(j).cbegin(),
		filts_.at(j).cend(), filtsFlat_.begin() + static_cast<ptrdiff_t>(j * filts_.at(j).size()));

	assert(is_aligned(filtsFlat_.data(), 64) && "CQT-basis filters: flat array is not aligned");

	if (quantile > 0) csr_ = make_unique<SparseMatrix>(reinterpret_cast<MKL_Complex8*>(filtsFlat_.data()),
		static_cast<int>(filts_.size()), static_cast<int>(filts_.front().size()), nNonZeros);

	filts_.clear();
}


void CqtBasis::ScaleFilters(const float scale)
{
	if (csr_) csr_->Scale(scale);
	else CHECK_IPP_RESULT(ippsMulC_32fc_I({ scale, 0 },
		reinterpret_cast<Ipp32fc*>(filtsFlat_.data()), static_cast<int>(filtsFlat_.size())));
}

void CqtBasis::RowMajorMultiply(const MKL_Complex8* src,
	MKL_Complex8* dest, const int nDestCols) const
{
	assert(nDestCols > 0 && "Number of columns in destination matrix must be positive");
	assert(filtsFlat_.size() == freqs_.size() * (nFft_ / 2 + 1));

	// If there are 99.9% of zeros, then mult time may be milliseconds instead of seconds:
	if (csr_) csr_->RowMajorMultiply(src, dest, nDestCols);
	else // If not, dense multiplication would be quicker:
	{
		complex<float> alpha(1), beta(0);
		cblas_cgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, static_cast<int>(freqs_.size()),
			nDestCols, static_cast<int>(nFft_ / 2 + 1), &alpha, filtsFlat_.data(),
			static_cast<int>(nFft_ / 2 + 1), src, nDestCols, &beta, dest, nDestCols);
	}
}