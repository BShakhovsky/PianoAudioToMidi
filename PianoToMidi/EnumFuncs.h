#pragma once

enum class WIN_FUNC { RECT, HANN, HAMMING, BLACKMAN, BLACKMAN_HARRIS, FLAT_TOP, KAISER, TRIAG };
std::shared_ptr<juce::dsp::WindowingFunction<float>>
GetWindowFunc(WIN_FUNC win, size_t len);

enum class PAD_MODE { CONSTANT, MIRROR, REPLICATE, WRAP };
std::function<IppStatus(const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize)>
GetPadFunc(PAD_MODE);

enum class AGGREGATE { MEAN, MIN, MAX, MEDIAN };
void Aggregate(const float* source, int srcNumRows, int rowStart,
	int srcNumColumns, float* dest, AGGREGATE);

enum class NORM_TYPE { NONE, L1, L2, INF };
std::function<IppStatus(const Ipp32f* src, int len, Ipp32f* normVal)>
GetNormFuncReal(NORM_TYPE);
std::function<IppStatus(const Ipp32fc* src, int len, Ipp32f* normVal)>
GetNormFuncComplex(NORM_TYPE);