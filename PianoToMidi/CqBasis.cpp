#include "stdafx.h"
#include "CqBasis.h"
#include "CqtError.h"

CqBasis::CqBasis(const int rate, const float fmin, const size_t nBins, const int octave, const int scale)
	: //rate_(rate), fmin_(fmin), nBins_(nBins), octave_(binsPerOctave), scale_(scale),
	freqs_(nBins), lens_(nBins)
{
	/* Filter bank described by McVicar, Matthew
	"A machine learning approach to automatic chord extraction."
	Dissertation, University of Bristol. 2013.*/

	using namespace std;

	assert(fmin > 0 && "Minimum frequency must be positive");
	assert(octave > 0 && "Bins per octave must be positive");
	assert(scale > 0 && "Filter scale must be positive");
	assert(nBins > 0 && "Number of bins must be positive");

	const auto Q(scale / (pow(2.f, 1.f / octave) - 1));

	for (size_t i(0); i < freqs_.size(); ++i)
		freqs_.at(i) = fmin * pow(2.f, static_cast<float>(i) / octave); // frequencies
	assert(freqs_.back() * (1 + 0.5 * HANN_BAND_WIDTH / Q) <= rate / 2. &&
		"Filter pass-band lies beyond Nyquist");

	const auto unusedIter(transform(freqs_.cbegin(), freqs_.cend(), lens_.begin(),
		[Q, rate](float freq) { return Q * rate / freq; }));
	assert(lens_.front() == *max_element(lens_.cbegin(), lens_.cend()));

	// All filters will be center-padded up to the nearest integral power of 2:
	filts_.assign(nBins, vector<complex<float>>(static_cast<size_t>(
		pow(2.f, ceil(log2(lens_.front())))), 0if));
	for (size_t i(0); i < filts_.size(); ++i)
	{
		const auto offset((filts_.at(i).size() - static_cast<size_t>(lens_.at(i))) / 2);
		for (size_t j(0); j < lens_.at(i); ++j) // length will be ceil(cqLen):
			filts_.at(i).at(j + offset) = exp(floor(j - lens_.at(i) / 2) / rate
				* 2if * static_cast<float>(M_PI) * freqs_.at(i));

		const auto srcDst(reinterpret_cast<Ipp32fc*>(filts_.at(i).data()) + offset);
		const auto buffSize(static_cast<int>(ceil(lens_.at(i))) + 1); // +1 to compensate for cqLen rounding

		string errMsg;
		auto status(ippsWinHann_32fc_I(srcDst, buffSize));
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
		status = ippsNormalize_32fc_I(srcDst, buffSize, { 0, 0 }, static_cast<Ipp32f>(norm));
		switch (status)
		{
		case ippStsNoErr:													break;
		case ippStsNullPtrErr:		errMsg = "buffer pointer is null";		break;
		case ippStsSizeErr:			errMsg = "length <= 0";					break;
		case ippStsDivByZeroErr:	errMsg = "L1-norm < min float number";	break;
		default:					errMsg = "unknown error";
		}
		if (not errMsg.empty()) throw CqtError(("Could not perform L1-norm: "
			+ errMsg + '\n' + ippGetStatusString(status)).c_str());
	}
}