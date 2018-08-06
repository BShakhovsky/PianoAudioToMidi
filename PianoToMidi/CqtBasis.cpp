#include "stdafx.h"
#include "CqtBasis.h"
#include "CqtError.h"

CqtBasis::CqtBasis(const int rate, const float fMin, const size_t nBins, const int octave,
	const int scale, const float sparsity, const int hopLen, const CQT_WINDOW window)
{
	/* Time-domain filter bank described by McVicar, Matthew
	"A machine learning approach to automatic chord extraction."
	Dissertation, University of Bristol. 2013.*/

	using namespace std;
	using juce::dsp::FFT;

	// Equivalent noise bandwidth (int FFT bins) of a window function:
	constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	assert(fMin > 0 && "Minimum frequency must be positive");
	assert(octave > 0 && "Bins per octave must be positive");
	assert(scale > 0 && "Filter scale must be positive");
	assert(nBins > 0 && "Number of bins must be positive");

	const auto Q(scale / (pow(2.f, 1.f / octave) - 1));

	std::vector<float> freqs(nBins);
	for (size_t i(0); i < freqs.size(); ++i)
		freqs.at(i) = fMin * pow(2.f, static_cast<float>(i) / octave); // frequencies
	assert(freqs.back() * (1 + 0.5 * WIN_BAND_WIDTH[static_cast<int>(window)] / Q) <= rate / 2. &&
		"Filter pass-band lies beyond Nyquist");

	std::vector<float> lens(nBins); // Fractional lengths of each filter
	const auto unusedIter(transform(freqs.cbegin(), freqs.cend(), lens.begin(),
		[Q, rate](float freq) { return Q * rate / freq; }));
	assert(lens.front() == *max_element(lens.cbegin(), lens.cend()));

	// All filters will be center-padded up to the nearest integral power of 2:
	auto nFft = static_cast<size_t>(pow(2.f, ceil(log2(lens.front()))));
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
		for (size_t j(0); j <= lens.at(i); ++j) // length will be ceil(cqLen)
			filts_.at(i).at(j + offset) = exp(floor(j - lens.at(i) / 2) / rate
				* 2if * static_cast<float>(M_PI) * freqs.at(i));

		const auto srcDst(reinterpret_cast<Ipp32fc*>(filts_.at(i).data()) + offset);
		const auto buffSize(static_cast<int>(ceil(lens.at(i))));

		string errMsg;
		IppStatus status(ippStsErr);
		switch (window)
		{
		case CQT_WINDOW::RECT:		status = ippStsNoErr;				break;
		case CQT_WINDOW::HANN:		status = ippsWinHann_32fc_I(
			// +1 if even to compensate for non-symmetry:
			srcDst, buffSize + 1 - static_cast<int>(lens.at(i)) % 2);	break;
		case CQT_WINDOW::HAMMING:	status = ippsWinHamming_32fc_I(
			srcDst, buffSize + 1 - static_cast<int>(lens.at(i)) % 2);	break;
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

		errMsg.clear();
		Ipp64f norm;
		status = ippsNorm_L1_32fc64f(srcDst, buffSize, &norm);
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
		status = ippsMulC_32fc_I({ lens.at(i) / nFft / static_cast<Ipp32f>(norm), 0 }, srcDst, buffSize);
		switch (status)
		{
		case ippStsNoErr:																break;
		case ippStsNullPtrErr:		errMsg = "buffer pointer is null";					break;
		case ippStsSizeErr:			errMsg = "length <= 0";								break;
//		case ippStsDivByZeroErr:	errMsg = "division by value < min float number";	break;
		default:					errMsg = "unknown error";
		}
		if (not errMsg.empty()) throw CqtError((
			"Could not re-normalize bases with respect to the FFT window length: "
			+ errMsg + '\n' + ippGetStatusString(status)).c_str());

		fft.perform(filts_.at(i).data(), filts_.at(i).data(), false);
		filts_.at(i).resize(nFft / 2); // Retain only the non-negative frequencies
	}

	// TODO: Sparsify the fft basis with quantile=sparsity
}