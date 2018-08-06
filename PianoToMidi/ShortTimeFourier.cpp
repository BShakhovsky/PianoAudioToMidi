#include "stdafx.h"
#include "ShortTimeFourier.h"

using namespace std;

vector<float> PadCentered(const vector<float>& buff, int padSize, bool isModeReflect)
{
	vector<float> result(buff.size() + padSize, 0);
	if (buff.empty()) return move(result);

	auto unusedIter(copy(buff.cbegin(), buff.cend(), result.begin() + padSize / 2));
	if (isModeReflect) // otherwise leave zeros as with constant pad mode
	{
		auto iter(result.begin() + padSize / 2);
		for (auto dist(distance(iter, result.end()) - padSize / 2 - 1); distance(result.begin(), iter) > dist;
			dist = distance(iter, result.end()) - padSize / 2 - 1)
		{
			unusedIter = reverse_copy(iter + 1, result.end() - padSize / 2, iter - dist);
			iter -= dist;
		}
		unusedIter = reverse_copy(iter + 1, iter + distance(result.begin(), iter) + 1, result.begin());

		iter = result.end() - padSize / 2;
		for (auto dist(distance(result.begin(), iter) - 1); distance(iter, result.end()) > dist;
			dist = distance(result.begin(), iter - 1))
		{
			unusedIter = reverse_copy(result.begin(), iter - 1, iter);
			iter += dist;
		}
		unusedIter = reverse_copy(iter - distance(iter, result.end()) - 1, iter - 1, iter);
	}

	return move(result);
}

ShortTimeFourier::ShortTimeFourier(const vector<float>& rawAudio,
	const int frameLen, int hopLen, const STFT_WINDOW window, const bool isPadReflected)
{
	using namespace juce::dsp;

	FFT fft(int(log2(frameLen)));
	assert(fft.getSize() == frameLen && "Frame length must be power of 2");

	if (not hopLen) hopLen = frameLen / 4;
	assert(hopLen > 0 && "Hop length must be positive and non-zero");

	unique_ptr<WindowingFunction<float>> fftWindow;
	switch (window)
	{
	case STFT_WINDOW::RECT: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::rectangular, false); break;
	case STFT_WINDOW::HANN: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::hann, false); break;
	case STFT_WINDOW::HAMMING: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::hamming, false); break;
	case STFT_WINDOW::BLACKMAN: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::blackman, false); break;
	case STFT_WINDOW::BLACKMAN_HARRIS: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::blackmanHarris, false); break;
	case STFT_WINDOW::FLAT_TOP: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::flatTop, false); break;
	case STFT_WINDOW::KAISER: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::kaiser, false); break;
	case STFT_WINDOW::TRIAG: fftWindow = make_unique<WindowingFunction<float>>(
		static_cast<size_t>(frameLen), WindowingFunction<float>::triangular, false); break;
	default: assert(!"Not all STFT windowing functions checked");
	}

	const auto paddedBuff(PadCentered(rawAudio, frameLen, isPadReflected)); // So that frames are centered
	assert(paddedBuff.size() >= static_cast<size_t>(frameLen) &&
		"PadCentered function must return at least frame length number of samples");

	// Vertical stride = 1 sample, horizontal stride = hop length:
	stft_.assign(1 + (paddedBuff.size() - frameLen) / hopLen, // The end may get truncated
		vector<complex<float>>(1ull + frameLen / 2)); // FFT will write here half + 1 complex numbers
	for (size_t i(0); i < stft_.size(); ++i)
	{
		// Temporarily window the time series into buffer with different type (complex instead of float)
		// FFT will overwrite result on top:
		CopyMemory(stft_.at(i).data(), paddedBuff.data() + static_cast<ptrdiff_t>(i) * hopLen,
			frameLen * sizeof paddedBuff.front()); // The last two floats (one complex) is currently empty

		fftWindow->multiplyWithWindowingTable(reinterpret_cast<float*>(
			stft_.at(i).data()), stft_.at(i).size() * 2);
		// Conjugate to match phase from DPWE code:
		fft.performRealOnlyForwardTransform(reinterpret_cast<float*>(stft_.at(i).data()), true);
	}
}