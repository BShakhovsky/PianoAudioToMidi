#include "stdafx.h"
#include "EnumFuncs.h"
#include "IntelCheckStatus.h"

using namespace std;
using juce::dsp::WindowingFunction;

shared_ptr<WindowingFunction<float>>
GetWindowFunc(const WIN_FUNC window, const size_t len)
{
	WindowingFunction<float>::WindowingMethod winMethod;
	switch (window)
	{
	case WIN_FUNC::RECT:			winMethod = WindowingFunction<float>::rectangular;		break;
	case WIN_FUNC::HANN:			winMethod = WindowingFunction<float>::hann;				break;
	case WIN_FUNC::HAMMING:			winMethod = WindowingFunction<float>::hamming;			break;
	case WIN_FUNC::BLACKMAN:		winMethod = WindowingFunction<float>::blackman;			break;
	case WIN_FUNC::BLACKMAN_HARRIS:	winMethod = WindowingFunction<float>::blackmanHarris;	break;
	case WIN_FUNC::FLAT_TOP:		winMethod = WindowingFunction<float>::flatTop;			break;
	case WIN_FUNC::KAISER:			winMethod = WindowingFunction<float>::kaiser;			break;
	case WIN_FUNC::TRIAG:			winMethod = WindowingFunction<float>::triangular;		break;
	default:						assert(!"Not all windowing functions checked");
									winMethod = WindowingFunction<float>::numWindowingMethods;
	}
	return make_shared<WindowingFunction<float>>(len, winMethod, false);
}

function<IppStatus(const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize)>
GetPadFunc(const PAD_MODE pad)
{
	switch (pad)
	{
	case PAD_MODE::CONSTANT:
		return [](const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize) { return
			ippiCopyConstBorder_32f_C1R_L(src, static_cast<int>(srcSize * sizeof *src),
				{ static_cast<int>(srcSize), 1 }, dest, static_cast<int>((srcSize + padSize)
					* sizeof *dest), { static_cast<int>(srcSize + padSize), 1 }, 0,
				static_cast<IppSizeL>(padSize / 2), 0); };
	case PAD_MODE::MIRROR:
		return [](const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize) { return
			ippiCopyMirrorBorder_32f_C1R_L(src, static_cast<int>(srcSize * sizeof *src),
				{ static_cast<int>(srcSize), 1 }, dest, static_cast<int>((srcSize + padSize)
					* sizeof *dest), { static_cast<int>(srcSize + padSize), 1 }, 0,
				static_cast<IppSizeL>(padSize / 2)); };
	case PAD_MODE::REPLICATE:
		return [](const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize) { return
			ippiCopyReplicateBorder_32f_C1R_L(src, static_cast<int>(srcSize * sizeof *src),
				{ static_cast<int>(srcSize), 1 }, dest, static_cast<int>((srcSize + padSize)
					* sizeof *dest), { static_cast<int>(srcSize + padSize), 1 }, 0,
				static_cast<IppSizeL>(padSize / 2)); };
	case PAD_MODE::WRAP:
		return [](const Ipp32f* src, size_t srcSize, Ipp32f* dest, size_t padSize) { return
			ippiCopyWrapBorder_32f_C1R_L(src, static_cast<int>(srcSize * sizeof *src),
				{ static_cast<int>(srcSize), 1 }, dest, static_cast<int>((srcSize + padSize)
					* sizeof *dest), { static_cast<int>(srcSize + padSize), 1 }, 0,
				static_cast<IppSizeL>(padSize / 2)); };
	default: assert(!"Not all pad modes checked"); return nullptr;
	}
}

void Aggregate(const float* src, const int srcNrows, const int rowStart,
	const int srcWidth, float* dest, const AGGREGATE aggr)
{
	function<IppStatus(const Ipp32f* src, int len, Ipp32f* resMeanMinMax)> AggrFunc = nullptr;

	switch (aggr)
	{
	case AGGREGATE::MEAN:	AggrFunc = [](const Ipp32f*pSrc, int len, Ipp32f* pMin)
	{ return ippsMean_32f(pSrc, len, pMin, ippAlgHintFast); };	break;
	case AGGREGATE::MIN:	AggrFunc = [](const Ipp32f*pSrc, int len, Ipp32f* pMin)
	{ return ippsMin_32f(pSrc, len, pMin); };					break;
	case AGGREGATE::MAX:	AggrFunc = [](const Ipp32f*pSrc, int len, Ipp32f* pMin)
	{ return ippsMax_32f(pSrc, len, pMin); };					break;
	case AGGREGATE::MEDIAN:
	{
		vector<double> destDouble(static_cast<size_t>(srcNrows)),
			srcDouble(src, src + static_cast<ptrdiff_t>(srcNrows) * srcWidth);

		Ipp32u buffSize;
		CHECK_IPP_RESULT(ippiFilterMedianGetBufferSize_64f({ 1, static_cast<int>(
			destDouble.size()) }, { srcWidth + srcWidth % 2 - 1, 1 }, 1, &buffSize));
		vector<Ipp8u> buff(buffSize);

		CHECK_IPP_RESULT(ippiFilterMedian_64f_C1R(srcDouble.data(), static_cast<int>(
			srcWidth * sizeof srcDouble.front()), destDouble.data(), static_cast<int>(
				sizeof destDouble.front()), { 1, static_cast<int>(destDouble.size()) },
			{ srcWidth + srcWidth % 2 - 1, 1 }, { 0, 0 }, buff.data()));

#pragma warning(suppress:4189) // Local variable is initialized but not referenced
		const auto unusedIter(transform(destDouble.cbegin(), destDouble.cend(),
			dest, [](double val) { return static_cast<float>(val); }));
	} break;
	default: assert(!"Not all aggregating functions checked");
	}
	if (AggrFunc) for (ptrdiff_t i(rowStart); i < srcNrows; ++i)
		CHECK_IPP_RESULT(AggrFunc(src + i * srcWidth, srcWidth, dest + i));
}

function<IppStatus(const Ipp32f* src, int len, Ipp32f* normVal)>
GetNormFuncReal(const NORM_TYPE norm)
{
	switch (norm)
	{
	case NORM_TYPE::NONE:	return nullptr;
	case NORM_TYPE::L1:		return [](const Ipp32f* src, int len, Ipp32f* normVal)
	{ return ippsNorm_L1_32f(src, len, normVal); };
	case NORM_TYPE::L2:		return[](const Ipp32f* src, int len, Ipp32f* normVal)
	{ return ippsNorm_L2_32f(src, len, normVal); };
	case NORM_TYPE::INF:	return [](const Ipp32f* src, int len, Ipp32f* normVal)
	{ return ippsNorm_Inf_32f(src, len, normVal); };
	default: assert(!"Not all normalization types checked"); return nullptr;
	}
}

function<IppStatus(const Ipp32fc* src, int len, Ipp32f* normVal)>
GetNormFuncComplex(const NORM_TYPE norm)
{
	switch (norm)
	{
	case NORM_TYPE::NONE:	return [](const Ipp32fc*, int, Ipp32f* normVal)
	{
		*normVal = 1;
		return ippStsNoErr;
	};
	case NORM_TYPE::L1:		return [](const Ipp32fc* src, int len, Ipp32f* normVal)
	{
		Ipp64f norm64;
		const auto status(ippsNorm_L1_32fc64f(src, len, &norm64));
		*normVal = static_cast<Ipp32f>(norm64);
		return status;
	};
	case NORM_TYPE::L2:		return [](const Ipp32fc* src, int len, Ipp32f* normVal)
	{
		Ipp64f norm64;
		const auto status(ippsNorm_L2_32fc64f(src, len, &norm64));
		*normVal = static_cast<Ipp32f>(norm64);
		return status;
	};
	case NORM_TYPE::INF:	return ippsNorm_Inf_32fc32f;
	default: assert(!"Not all normalization types checked"); return nullptr;
	}
}