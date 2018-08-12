#pragma once

class HarmonicPercussive
{
public:
	explicit HarmonicPercussive(const std::shared_ptr<class ConstantQ>&,
		int kernelHarm = 31, int kernelPerc = 31, float power = 2.f,
		float marginHarm = 1.f, float marginPerc = 1.f);

	const std::vector<float>& GetHarmonic() const { return harm_; }
	const std::vector<float>& GetPercussive() const { return perc_; }
private:
	std::vector<float> harm_, perc_;
};