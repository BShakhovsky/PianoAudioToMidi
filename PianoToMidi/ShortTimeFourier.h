#pragma once

class ShortTimeFourier
{
public:
	enum class STFT_WINDOW { RECT, HANN, HAMMING, BLACKMAN, BLACKMAN_HARRIS, FLAT_TOP, KAISER, TRIAG };

	explicit ShortTimeFourier(const std::vector<float>& rawAudio, int frameLen = 2'048,
		int hopLen = 0, STFT_WINDOW window = STFT_WINDOW::HANN, bool isPadReflected = true);
	~ShortTimeFourier();

	std::vector<std::vector<std::complex<float>>> RealForward() const;
private:
	const std::unique_ptr<struct StftData> data_;

	ShortTimeFourier(const ShortTimeFourier&) = delete;
	ShortTimeFourier operator=(const ShortTimeFourier&) = delete;
};