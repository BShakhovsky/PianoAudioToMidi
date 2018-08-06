#include "stdafx.h"
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
	bInd_(0), offset_(0), buff_(nullptr), size_(0)
{
	using juce::dsp::FFT;

	assert(nBins > 0 && "Number of bins must be positive");

	CalcFrequencies(fMin, octave, scale);
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

	// TODO: Sparsify the fft basis with quantile=sparsity
	UNREFERENCED_PARAMETER(sparsity);
}


void CqtBasisData::CalcFrequencies(const float fMin, const int octave, const int scale)
{
	assert(fMin > 0 && "Minimum frequency must be positive");
	assert(octave > 0 && "Bins per octave must be positive");
	assert(scale > 0 && "Filter scale must be positive");

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