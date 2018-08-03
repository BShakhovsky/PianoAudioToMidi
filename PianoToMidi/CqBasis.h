#pragma once

class CqBasis
{
	// Equivalent noise bandwidth (int FFT bins) of a window function:
	static constexpr auto HANN_BAND_WIDTH = 1.50018310546875;
public:
	explicit CqBasis(int sampleRate, float fmin,
		size_t nBins = 88, int binsPerOctave = 12, int filterScale = 1);
	const std::vector<float>& GetCqLengths() const { return lens_; }
	const std::vector<std::vector<std::complex<float>>>& GetCqFilters() const { return filts_; }
private:
//	const int rate_;
//	const float fmin_;
//	const size_t nBins_;
//	const int octave_;
//	const int scale_;

	std::vector<float> freqs_, lens_; // nBins number of fractional length of each filter
	std::vector<std::vector<std::complex<float>>> filts_; // time-domain
};