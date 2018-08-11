#pragma once

class HarmonicPercussive
{
public:
	explicit HarmonicPercussive(const std::shared_ptr<class ConstantQ>&,
		int kernelSize = 31, float power = 2.f, bool toMask = false, float margin = 1.f);
	~HarmonicPercussive();
};