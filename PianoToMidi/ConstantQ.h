#pragma once

class ConstantQ
{
	const double BW_FASTEST = .85; // Resampling bandwidth (rolloff) = 85% of Nyquist
public:
	enum class NORM_TYPE { NONE, L1, L2, INF };
	enum class CQT_WINDOW { RECT, HANN, HAMMING };
	// Equivalent noise bandwidth (int FFT bins) of a window function:
	static constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	explicit ConstantQ(const std::shared_ptr<class AudioLoader>& audio, int hopLength = 512,
		float fMin = 27.5f, size_t nBins = 88, int binsPerOctave = 12,
		float filterScale = 1, NORM_TYPE norm = NORM_TYPE::L1, float sparsity = .01f,
		CQT_WINDOW windowFunc = CQT_WINDOW::HANN, bool toScale = true, bool isPadReflect = true);
	~ConstantQ();
private:
	void EarlyDownsample(bool isKaiserFast, int nOctaves, double nyquist, double filterCutoff, bool toScale);
	void Response(bool isPadReflect) const;
	void TrimStack(size_t nBins);
	void Scale(int sampleRateInitial, float fMin, size_t nBins, bool toScale) const;

	const std::unique_ptr<class CqtBasis> qBasis_;

	std::shared_ptr<AudioLoader> audio_;
	int hopLen_;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#elif not defined _WIN32
#	error It should be either 32- or 64-bit Windows
#endif
	std::vector<std::vector<std::complex<float>>> cqtResp_;

	ConstantQ(const ConstantQ&) = delete;
	ConstantQ operator=(const ConstantQ&) = delete;
};