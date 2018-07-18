#include "stdafx.h"
#include "FFmpegError.h"

FFmpegError::FFmpegError(const char* msg) noexcept : errMsg_(msg) {} // 4710 function not inlined