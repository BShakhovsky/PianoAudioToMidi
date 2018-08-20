#include "stdafx.h"
#include "AlignedVector.h"
#include "EnumTypes.h"
#include "ShortTimeFourier.h"

using namespace std;
using namespace juce::dsp;

ShortTimeFourier::ShortTimeFourier(const size_t frameLen,
	const WIN_FUNC window, const bool isPadReflected) noexcept
	: frameLen_(frameLen), isPadReflect_(isPadReflected),
	fft_(make_unique<FFT>(int(log2(frameLen)))),
	nFrames_(0ull), nFreqs_(frameLen / 2 + 1)
{
	assert(static_cast<size_t>(fft_->getSize()) == frameLen && "Frame length must be power of 2");
	GetStftWindow(window);
	assert(window_ && "Could not get STFT windowing function");
}

ShortTimeFourier::~ShortTimeFourier() {}

void ShortTimeFourier::RealForward(const float* rawAudio, const size_t nSamples, int hopLen)
{
#ifdef _DEBUG
	using boost::alignment::is_aligned;
#endif
	if (hopLen == 0) hopLen = static_cast<int>(frameLen_) / 4;
	assert(hopLen && "Hop length must be non-zero");

	AlignedVector<float> paddedBuff;
	// Pad (frame length / 2) both sides, so that frames are centered:
	PadCentered(rawAudio, nSamples, &paddedBuff, isPadReflect_);
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

		window_->multiplyWithWindowingTable(reinterpret_cast<float*>(
			stft_.data() + i * nFreqs_), frameLen_);
		// Conjugate to match phase from DPWE code:
		fft_->performRealOnlyForwardTransform(reinterpret_cast<float*>(
			stft_.data() + i * nFreqs_), true);
	}

	MKL_Cimatcopy('R', 'T', nFrames_, nFreqs_, { 1, 0 },
		reinterpret_cast<MKL_Complex8*>(stft_.data()), nFreqs_, nFrames_);
}

void ShortTimeFourier::PadCentered(const float* src, const size_t srcSize,
	AlignedVector<float>* dest, const bool isModeReflect) const
{
	dest->assign(srcSize + frameLen_, 0);
	if (srcSize == 0) return;

	const auto fLen(static_cast<ptrdiff_t>(frameLen_));
	auto unusedIter(copy(src, src + static_cast<ptrdiff_t>(srcSize), dest->begin() + fLen / 2));
	if (not isModeReflect) return; // Leave zeros as with constant pad mode

	auto iter(dest->begin() + fLen / 2);
	for (auto dist(distance(iter, dest->end()) - fLen / 2 - 1); distance(dest->begin(), iter) > dist;
		dist = distance(iter, dest->end()) - fLen / 2 - 1)
	{
		unusedIter = reverse_copy(iter + 1, dest->end() - fLen / 2, iter - dist);
		iter -= dist;
	}
	unusedIter = reverse_copy(iter + 1, iter + distance(dest->begin(), iter) + 1, dest->begin());

	iter = dest->end() - fLen / 2;
	for (auto dist(distance(dest->begin(), iter) - 1); distance(iter, dest->end()) > dist;
		dist = distance(dest->begin(), iter - 1))
	{
		unusedIter = reverse_copy(dest->begin(), iter - 1, iter);
		iter += dist;
	}
	unusedIter = reverse_copy(iter - distance(iter, dest->end()) - 1, iter - 1, iter);
}

void ShortTimeFourier::GetStftWindow(const WIN_FUNC window)
{
	WindowingFunction<float>::WindowingMethod winFunc;
	switch (window)
	{
	case WIN_FUNC::RECT:			winFunc = WindowingFunction<float>::rectangular;	break;
	case WIN_FUNC::HANN:			winFunc = WindowingFunction<float>::hann;			break;
	case WIN_FUNC::HAMMING:			winFunc = WindowingFunction<float>::hamming;		break;
	case WIN_FUNC::BLACKMAN:		winFunc = WindowingFunction<float>::blackman;		break;
	case WIN_FUNC::BLACKMAN_HARRIS:	winFunc = WindowingFunction<float>::blackmanHarris;	break;
	case WIN_FUNC::FLAT_TOP:		winFunc = WindowingFunction<float>::flatTop;		break;
	case WIN_FUNC::KAISER:			winFunc = WindowingFunction<float>::kaiser;			break;
	case WIN_FUNC::TRIAG:			winFunc = WindowingFunction<float>::triangular;		break;
	default:						assert(!"Not all windowing functions checked");
									winFunc = WindowingFunction<float>::numWindowingMethods;
	}
	window_ = make_unique<WindowingFunction<float>>(static_cast<size_t>(frameLen_), winFunc, false);
}