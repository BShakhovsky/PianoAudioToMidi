#pragma once

class ConstantQ
{
	static constexpr double BW_FASTEST = .85; // Resampling bandwidth (rolloff) = 85% of Nyquist
public:
	enum class CQT_WINDOW { RECT, HANN, HAMMING };
	// Equivalent noise bandwidth (int FFT bins) of a window function:
	static constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	explicit ConstantQ(const std::shared_ptr<class AudioLoader>& audio,
		size_t nBins = 88, int binsPerOctave = 12, float fMin = 27.5f, int hopLength = 512,
		float filterScale = 1, NORM_TYPE norm = NORM_TYPE::L1, float sparsity = .01f,
		CQT_WINDOW windowFunc = CQT_WINDOW::HANN, bool toScale = true, PAD_MODE pad = PAD_MODE::MIRROR);
	~ConstantQ();
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	const std::shared_ptr<AlignedVector<float>>& GetCQT() const { return cqt_; }
	size_t GetNumBins() const { return nBins_; }

	int GetBinsPerOctave() const { return octave_; }
	float GetMinFrequency() const { return fMin_; }

	size_t GetFftFrameLength() const;
	int GetHopLength() const { return hopLen_; }
	int GetSampleRate() const { return rateInitial_; }
#pragma warning(pop)
private:
	void EarlyDownsample(bool isKaiserFast, int nOctaves, double nyquist, double filterCutoff);
	void HalfDownSample(int nOctaves);
	void Response();
	void TrimErrors();
	void Scale(int sampleRateInitial, bool toScale);

	const size_t nBins_;
	const float fMin_;
	const int octave_, hopLen_;
	int hopLenReduced_, rateInitial_;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#endif
	const std::unique_ptr<class CqtBasis> qBasis_;
	std::unique_ptr<class ShortTimeFourier> stft_;
	std::shared_ptr<AudioLoader> audio_;

	std::vector<std::vector<float>> cqtResp_;
	std::shared_ptr<AlignedVector<float>> cqt_;
#if not defined _WIN64 and defined NDEBUG
	const byte pad_[4]{ 0 };
#endif

	ConstantQ(const ConstantQ&) = delete;
	const ConstantQ& operator=(const ConstantQ&) = delete;
};