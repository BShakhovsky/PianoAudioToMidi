#pragma once

class ShortTimeFourier
{
public:
	explicit ShortTimeFourier(size_t frameLen = 2'048,
		WIN_FUNC window = WIN_FUNC::HANN, PAD_MODE pad = PAD_MODE::MIRROR);
	~ShortTimeFourier();

	void RealForward(const float* rawAudio, size_t nSamples, int hopLen = 0);
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	const AlignedVector<std::complex<float>>& GetSTFT() const { return stft_; }
	size_t GetNumFrames() const { return nFrames_; }
#pragma warning(pop)
private:
	const size_t frameLen_;
	const std::unique_ptr<juce::dsp::FFT> fft_;
	std::shared_ptr<juce::dsp::WindowingFunction<float>> WinFunc_;
	std::function<IppStatus(const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize)> PadFunc_;

	AlignedVector<std::complex<float>> stft_;
	size_t nFrames_, nFreqs_;
#if not defined _WIN64 and defined NDEBUG
	const byte pad_[4]{ 0 };
#endif

	ShortTimeFourier(const ShortTimeFourier&) = delete;
	const ShortTimeFourier& operator=(const ShortTimeFourier&) = delete;
};