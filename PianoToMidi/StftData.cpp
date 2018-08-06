#include "stdafx.h"
#include "ShortTimeFourier.h"
#include "StftData.h"

using namespace std;
using juce::dsp::WindowingFunction;
typedef ShortTimeFourier::STFT_WINDOW WIN_FUNC;

StftData::StftData(const vector<float>& rawAudio, const int frameLen, const int hopLen,
	const ShortTimeFourier::STFT_WINDOW window, const bool isPadReflected)
	: frameLen_(frameLen), hopLen_(hopLen ? hopLen : frameLen / 4), fft_(int(log2(frameLen)))
{
	assert(fft_.getSize() == frameLen && "Frame length must be power of 2");
	assert(hopLen_ > 0 && "Hop length must be positive and non-zero");

	GetStftWindow(window);
	assert(window_ && "Could not get STFT windowing function");

	PadCentered(rawAudio, isPadReflected); // Pad (frame length / 2) both sides, so that frames are centered
	assert(paddedBuff_.size() >= static_cast<size_t>(frameLen) &&
		"PadCentered function must return at least frame length number of samples");
}

void StftData::PadCentered(const vector<float>& buff, const bool isModeReflect)
{
	paddedBuff_.assign(buff.size() + frameLen_, 0);
	if (buff.empty()) return;

	auto unusedIter(copy(buff.cbegin(), buff.cend(), paddedBuff_.begin() + frameLen_ / 2));
	if (not isModeReflect) return; // Leave zeros as with constant pad mode

	auto iter(paddedBuff_.begin() + frameLen_ / 2);
	for (auto dist(distance(iter, paddedBuff_.end()) - frameLen_ / 2 - 1);
		distance(paddedBuff_.begin(), iter) > dist;
		dist = distance(iter, paddedBuff_.end()) - frameLen_ / 2 - 1)
	{
		unusedIter = reverse_copy(iter + 1, paddedBuff_.end() - frameLen_ / 2, iter - dist);
		iter -= dist;
	}
	unusedIter = reverse_copy(iter + 1, iter
		+ distance(paddedBuff_.begin(), iter) + 1, paddedBuff_.begin());

	iter = paddedBuff_.end() - frameLen_ / 2;
	for (auto dist(distance(paddedBuff_.begin(), iter) - 1);
		distance(iter, paddedBuff_.end()) > dist;
		dist = distance(paddedBuff_.begin(), iter - 1))
	{
		unusedIter = reverse_copy(paddedBuff_.begin(), iter - 1, iter);
		iter += dist;
	}
	unusedIter = reverse_copy(iter - distance(iter, paddedBuff_.end()) - 1, iter - 1, iter);
}

void StftData::GetStftWindow(const WIN_FUNC window)
{
	switch (window)
	{
	case WIN_FUNC::RECT:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::rectangular, false);		break;
	case WIN_FUNC::HANN:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::hann, false);				break;
	case WIN_FUNC::HAMMING:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::hamming, false);			break;
	case WIN_FUNC::BLACKMAN:		window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::blackman, false);			break;
	case WIN_FUNC::BLACKMAN_HARRIS:	window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::blackmanHarris, false);	break;
	case WIN_FUNC::FLAT_TOP:		window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::flatTop, false);			break;
	case WIN_FUNC::KAISER:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::kaiser, false);			break;
	case WIN_FUNC::TRIAG:			window_ = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen_), WindowingFunction<float>::triangular, false);		break;
	default: assert(!"Not all STFT windowing functions checked"); window_ = nullptr;
	}
}