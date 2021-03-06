#pragma once
#include "targetver.h"
#pragma warning(disable:5045 5204 26812)

#pragma warning(push, 3)
#	include <Windows.h>
#	include <CommCtrl.h>
#	include <gdiplus.h>
#pragma warning(pop)
#pragma comment(lib, "Gdiplus.lib")
#include <WindowsX.h>

#include <cassert>
#pragma warning(push, 3)
//#pragma warning(disable:4244 4365 4514 4571 4625 4626 4710 4711 4774 4820 5026 5027)
//#	include <codecvt>
#pragma warning(disable:4244 4365)
#	include <regex>
#pragma warning(pop)

#pragma warning(push, 3)
#	include <ippcore.h>
#	include <ippvm.h>
#	include <ipps.h>
#	include <ippi.h>

#	include <mkl_trans.h>
#	include <mkl_cblas.h>
#	include <mkl_spblas.h>
#	include <mkl_vml_functions.h>
#pragma warning(pop)

#pragma warning(push, 3)
#pragma warning(disable:4242 4365 4514 4571 4619 4625 4626 4668 4710 4711 4774 4820 4826 4866 5026 5027 5031 26439 26495 28251)
#	include <boost/filesystem/operations.hpp>
#	include <boost/archive/binary_iarchive.hpp>
#	include <boost/serialization/array.hpp>
#	include <boost/align/aligned_allocator.hpp>
#	ifdef _DEBUG
#		include <boost/align/is_aligned.hpp>
#	endif
#pragma warning(pop)

// https://www.gyan.dev/ffmpeg/builds/
// https://web.archive.org/web/20200918014242/https://ffmpeg.zeranoe.com/builds/
extern "C"
{
#pragma warning(push, 2)
#	include <libavformat/avformat.h>
#	include <libswresample/swresample.h>
#pragma warning(pop)
}
#pragma comment(lib, "avcodec")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#	pragma comment(lib, "Bcrypt") // required by AvUtil
#pragma comment(lib, "swresample")

#pragma warning(push, 2)
#pragma warning(disable:4365 4514 4571 4625 4626 4710 4711 4774 4820 5026 5027 5219 26434 26439 26451 26495 26819)
#	include <Juce/AppConfig.h>
#	include <juce_dsp/juce_dsp.h>
//#	include <juce_audio_basics/juce_audio_basics.h>
#pragma warning(pop)
#pragma comment(lib, "Juce")

#pragma warning(push, 0)
#pragma warning(disable:4355 4514 4571 4623 4625 4626 4701 4702 4710 4711 4714 4820 5026 5027 5039 5045 6001 6255 6269 6294)
#pragma warning(disable:26110 26115 26117 26439 26444 26450 26451 26454 26495 26498 26819 26820 28020)
#	include <fdeep/fdeep.hpp>
#pragma warning(pop)

template <typename QUOTIENT, typename DIVISIBLE, typename DIVIDER>
QUOTIENT Divide(DIVISIBLE divisible, DIVIDER divider) { return static_cast<QUOTIENT>(divisible) / static_cast<QUOTIENT>(divider); }

template<typename DIVISIBLE, typename DIVIDER>
float Divide(DIVISIBLE divisible, DIVIDER divider) { return Divide<float, DIVISIBLE, DIVIDER>(divisible, divider); }

template<typename LEFT, typename RIGHT>
float Multiply(LEFT left, RIGHT right) { return static_cast<float>(left) * static_cast<float>(right); }