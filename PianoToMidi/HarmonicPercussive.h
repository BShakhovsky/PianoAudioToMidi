#pragma once

class HarmonicPercussive
{
public:
	explicit HarmonicPercussive(const std::shared_ptr<class ConstantQ>&,
		int kernelHarm = 31, int kernelPerc = 31, float power = 2.f,
		float marginHarm = 1.f, float marginPerc = 1.f);
	~HarmonicPercussive();

	void OnsetEnvelope(size_t lag = 1, int maxSize = 1, bool toDetrend = false,
		bool toCenter = true, AGGREGATE aggregate = AGGREGATE::MEAN);
	void OnsetPeaksDetect(bool toBackTrack = false);

	void Chromagram(bool baseC = true, // the first chroma bin will start at 'C', else at 'A'
		NORM_TYPE norm = NORM_TYPE::INF, float threshold = 0.f, // if zero, threshold not performed
		// to convolve the CQT to chroma filter bank:
		size_t nChromaOutput = 12, WIN_FUNC window = WIN_FUNC::RECT);
	void ChromaSum(bool onsetsOnly = true);
	std::string KeySignature() const;

#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	const std::vector<float>& GetHarmonic() const { return harm_; }
	const std::vector<float>& GetOnsetEnvelope() const { return percEnv_; }
	const std::vector<size_t>& GetOnsetPeaks() const { return percPeaks_; }
#pragma warning(pop)
private:
	void OnsetBackTrack();

	std::shared_ptr<ConstantQ> cqt_;

	std::vector<float> harm_, perc_, percEnv_;
	std::vector<size_t> percPeaks_;

	const byte pad_[sizeof(intptr_t) - sizeof(bool)]{ 0 };
	bool baseC_;
	size_t nChroma_;
	AlignedVector<float> chroma_;
	std::vector<float> chrSum_;

	const HarmonicPercussive& operator=(const HarmonicPercussive&) = delete;
};