#include "stdafx.h"
#include "AlignedVector.h"
#include "ShortTimeFourier.h"

using namespace std;
using namespace juce::dsp;

ShortTimeFourier::ShortTimeFourier(const size_t frameLen,
	const STFT_WINDOW window, const bool isPadReflected) noexcept
	: frameLen_(frameLen), isPadReflect_(isPadReflected), fft_(make_unique<FFT>(int(log2(frameLen)))),
	nFrames_(0ull), nFreqs_(0ull)
{
	assert(static_cast<size_t>(fft_->getSize()) == frameLen && "Frame length must be power of 2");
	GetStftWindow(window);
	assert(window_ && "Could not get STFT windowing function");
}

ShortTimeFourier::~ShortTimeFourier() {}

void ShortTimeFourier::RealForward(const vector<float>& rawAudio, int hopLen)
{
#ifdef _DEBUG
	using boost::alignment::is_aligned;
#endif
	if (hopLen == 0) hopLen = static_cast<int>(frameLen_) / 4;
	assert(hopLen && "Hop length must be non-zero");

	AlignedVector<float> paddedBuff;
	// Pad (frame length / 2) both sides, so that frames are centered:
	PadCentered(rawAudio, &paddedBuff, isPadReflect_);
	assert(paddedBuff.size() >= frameLen_ &&
		"PadCentered function must return at least frame length number of samples");
	assert(is_aligned(paddedBuff.data(), 64) and "Padded audio buffer is not aligned");

	// Vertical stride = 1 sample, horizontal stride = hop length, the end may get truncated:
	nFrames_ = (paddedBuff.size() - frameLen_) / hopLen + 1;
	nFreqs_ = frameLen_ / 2 + 1;
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
}

void ShortTimeFourier::PadCentered(const vector<float>& src,
	AlignedVector<float>* dest, const bool isModeReflect) const
{
	dest->assign(src.size() + frameLen_, 0);
	if (src.empty()) return;

	const auto fLen(static_cast<ptrdiff_t>(frameLen_));
	auto unusedIter(copy(src.cbegin(), src.cend(), dest->begin() + fLen / 2));
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

void ShortTimeFourier::GetStftWindow(const STFT_WINDOW window)
{
	switch (window)
	{
	case STFT_WINDOW::RECT:				window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::rectangular, false);		break;
	case STFT_WINDOW::HANN:				window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::hann, false);				break;
	case STFT_WINDOW::HAMMING:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::hamming, false);			break;
	case STFT_WINDOW::BLACKMAN:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::blackman, false);			break;
	case STFT_WINDOW::BLACKMAN_HARRIS:	window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::blackmanHarris, false);	break;
	case STFT_WINDOW::FLAT_TOP:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::flatTop, false);			break;
	case STFT_WINDOW::KAISER:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::kaiser, false);			break;
	case STFT_WINDOW::TRIAG:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::triangular, false);		break;
	default: assert(!"Not all STFT windowing functions checked"); window_ = nullptr;
	}
}