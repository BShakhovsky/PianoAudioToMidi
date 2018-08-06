#pragma once

struct StftData
{
	const int frameLen_, hopLen_;

	juce::dsp::FFT fft_;
	std::vector<float> paddedBuff_;
	std::unique_ptr<juce::dsp::WindowingFunction<float>> window_;

	explicit StftData(const std::vector<float>& rawAudio, int frameLen, int hopLen,
		ShortTimeFourier::STFT_WINDOW window, bool isPadReflected);
private:
	void PadCentered(const std::vector<float>& buff, bool isModeReflect);
	void GetStftWindow(ShortTimeFourier::STFT_WINDOW window);

	StftData(const StftData&) = delete;
	StftData operator=(const StftData&) = delete;
};