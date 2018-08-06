#pragma once

class ShortTimeFourier
{
	enum class STFT_WINDOW { RECT, HANN, HAMMING, BLACKMAN, BLACKMAN_HARRIS, FLAT_TOP, KAISER, TRIAG };
public:
	explicit ShortTimeFourier(const std::vector<float>& rawAudio, int frameLen = 2'048,
		int hopLen = 0, STFT_WINDOW window = STFT_WINDOW::HANN, bool isPadReflected = true);
	const std::vector<std::vector<std::complex<float>>>& GetSTFT() const { return stft_; }
private:
	std::vector<std::vector<std::complex<float>>> stft_;

	ShortTimeFourier(const ShortTimeFourier&) = delete;
	ShortTimeFourier operator=(const ShortTimeFourier&) = delete;
};