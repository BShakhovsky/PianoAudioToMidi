#pragma once

class ConstantQ
{
	const double BW_FASTEST = .85; // Resampling bandwidth (rolloff) = 85% of Nyquist
public:
	enum class NORM_TYPE { NONE, L1, L2, INF };
	enum class CQT_WINDOW { RECT, HANN, HAMMING };
	// Equivalent noise bandwidth (int FFT bins) of a window function:
	static constexpr double WIN_BAND_WIDTH[] = { 1., 1.50018310546875, 1.3629455320350348 };

	explicit ConstantQ(const std::shared_ptr<class AudioLoader>& audio,
		size_t nBins = 88, int binsPerOctave = 12, float fMin = 27.5f, int hopLength = 512,
		float filterScale = 1, NORM_TYPE norm = NORM_TYPE::L1, float sparsity = .01f,
		CQT_WINDOW windowFunc = CQT_WINDOW::HANN, bool toScale = true, bool isPadReflect = true);
	~ConstantQ();

	void Amplitude2power();
	void TrimSilence(float aMin = 1e-10f, float topDb = 60.f);
	void Power2db(float ref = 1.f, float aMin = 1e-10f, float topDb = 80.f) {
		Power2db_helper(cqt_.data(), static_cast<int>(cqt_.size()), ref, aMin, topDb); }

	const AlignedVector<float>& GetCQT() const { return cqt_; }
	size_t GetNumBins() const { return nBins_; }

	size_t GetFftFrameLength() const;
	int GetHopLength() const { return hopLen_; }
private:
	void EarlyDownsample(bool isKaiserFast, int nOctaves, double nyquist, double filterCutoff);
	void HalfDownSample(int nOctaves);
	void Response();
	void TrimErrors();
	void Scale(int sampleRateInitial, float fMin, bool toScale);

	static void Power2db_helper(float* powerSpectrum, int size, float ref, float aMin, float topDb);

	const size_t nBins_;
	const int hopLen_;
	int hopLenReduced_;

	const std::unique_ptr<class CqtBasis> qBasis_;
	std::unique_ptr<class ShortTimeFourier> stft_;
	std::shared_ptr<AudioLoader> audio_;

	std::vector<std::vector<float>> cqtResp_;
	AlignedVector<float> cqt_;

	ConstantQ(const ConstantQ&) = delete;
	ConstantQ operator=(const ConstantQ&) = delete;
};