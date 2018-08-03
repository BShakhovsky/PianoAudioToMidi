#pragma once

class ShortTimeFourier
{
public:
	explicit ShortTimeFourier(const std::vector<float>& rawAudio,
		int frameLen = 2'048, int hopLen = 0, bool isWindowHann = true, bool isPadReflected = true);
	const std::vector<std::vector<std::complex<float>>>& GetSTFT() const { return stft_; }
private:
	std::vector<std::vector<std::complex<float>>> stft_;

	ShortTimeFourier(const ShortTimeFourier&) = delete;
	ShortTimeFourier operator=(const ShortTimeFourier&) = delete;
};