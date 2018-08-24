#pragma once

class Tempogram
{
public:
	float MostProbableTempo(const std::vector<float>& onsetEnvelope, int sampleRate, int hopLength,
		int startBpm = 120, float stdBpm = 1.f, float acSize = 8.f,
		float maxTempo = 320.f, // if zero, no threshold will be performed
		AGGREGATE aggr = AGGREGATE::MEAN);
private:
	void Calculate(const std::vector<float>& onsetEnvelope, int winLength = 384,
		bool toCenter = true, WIN_FUNC window = WIN_FUNC::HANN, NORM_TYPE norm = NORM_TYPE::INF);

	std::vector<std::vector<float>> autoCorr2D_;
};