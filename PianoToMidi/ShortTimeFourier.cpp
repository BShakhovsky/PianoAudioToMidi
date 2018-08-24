#include "stdafx.h"
#include "AlignedVector.h"
#include "EnumFuncs.h"
#include "ShortTimeFourier.h"
#include "IntelCheckStatus.h"

using namespace std;
using namespace juce::dsp;

ShortTimeFourier::ShortTimeFourier(const size_t frameLen,
	const WIN_FUNC window, const PAD_MODE pad)
	: frameLen_(frameLen), fft_(make_unique<FFT>(int(log2(frameLen)))),
	nFrames_(0ull), nFreqs_(frameLen / 2 + 1)
{
	assert(static_cast<size_t>(fft_->getSize()) == frameLen && "Frame length must be power of 2");
	WinFunc_ = GetWindowFunc(window, static_cast<size_t>(frameLen));
	PadFunc_ = GetPadFunc(pad);
}

ShortTimeFourier::~ShortTimeFourier() {}

void ShortTimeFourier::RealForward(const float* rawAudio, const size_t nSamples, int hopLen)
{
#ifdef _DEBUG
	using boost::alignment::is_aligned;
#endif
	if (hopLen == 0) hopLen = static_cast<int>(frameLen_) / 4;
	assert(hopLen && "Hop length must be non-zero");

	// Pad (frame length / 2) both sides, so that frames are centered:
	AlignedVector<float> paddedBuff(nSamples + frameLen_);
	CHECK_IPP_RESULT(PadFunc_(rawAudio, nSamples, paddedBuff.data(), frameLen_));
	assert(paddedBuff.size() >= frameLen_ &&
		"PadCentered function must return at least frame length number of samples");
	assert(is_aligned(paddedBuff.data(), 64) and "Padded audio buffer is not aligned");

	// Vertical stride = 1 sample, horizontal stride = hop length, the end may get truncated:
	nFrames_ = (paddedBuff.size() - frameLen_) / hopLen + 1;
	stft_.resize(nFrames_ * nFreqs_); // FFT will write here half + 1 complex numbers
	// now it is columns, but will be rows after transpose
	for (ptrdiff_t i(0); i < static_cast<ptrdiff_t>(nFrames_); ++i)
	{
		// Temporarily window the time series into buffer with different type (complex instead of float)
		// FFT will overwrite result on top:
		CopyMemory(stft_.data() + i * nFreqs_, paddedBuff.data() + i * hopLen,
			// The last two floats (one complex) is currently empty:
			frameLen_ * sizeof paddedBuff.front());

		WinFunc_->multiplyWithWindowingTable(reinterpret_cast<float*>(
			stft_.data() + i * nFreqs_), frameLen_);
		// Conjugate to match phase from DPWE code:
		fft_->performRealOnlyForwardTransform(reinterpret_cast<float*>(
			stft_.data() + i * nFreqs_), true);
	}

	MKL_Cimatcopy('R', 'T', nFrames_, nFreqs_, { 1, 0 },
		reinterpret_cast<MKL_Complex8*>(stft_.data()), nFreqs_, nFrames_);
}