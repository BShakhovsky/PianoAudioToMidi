#pragma once

class HarmonicPercussive
{
public:
	explicit HarmonicPercussive(const std::shared_ptr<class ConstantQ>&,
		int kernelHarm = 31, int kernelPerc = 31, float power = 2.f,
		float marginHarm = 1.f, float marginPerc = 1.f);

	enum class AGGREGATE { MEAN, MIN, MAX, MEDIAN };
	void OnsetEnvelope(size_t lag = 1, int maxSize = 1, bool toDetrend = false,
		bool toCenter = true, AGGREGATE aggregate = AGGREGATE::MEAN);
	void OnsetPeaksDetect(bool toBackTrack = false);

	const std::vector<float>& GetHarmonic() const { return harm_; }
	const std::vector<float>& GetPercussive() const { return perc_; }

	const std::vector<float>& GetOnsetEnvelope() const { return percEnv_; }
	const std::vector<size_t>& GetOnsetPeaks() const { return percPeaks_; }
private:
	void OnsetBackTrack();

	std::shared_ptr<ConstantQ> cqt_;

	std::vector<float> harm_, perc_, percEnv_;
	std::vector<size_t> percPeaks_;
};