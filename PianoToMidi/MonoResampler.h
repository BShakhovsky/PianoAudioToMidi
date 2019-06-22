#pragma once
#pragma warning(push)
#pragma warning(disable:4710) // FFmpegError(const char*): function not inlined

class MonoResampler
{
public:
#pragma warning(push)
#pragma warning(disable: 4514)	// Unreferenced inline function has been removed
#pragma warning(disable: 26439)	// This kind of function may not throw. Declare it 'noexcept' (f.6)
	MonoResampler() : context_(swr_alloc()), srcData_(nullptr), dstData_(nullptr)
	{
		if (!context_) throw FFmpegError("Could not allocate resampler context");
	}
#pragma warning(pop)
	~MonoResampler();

	std::pair<uint8_t*, size_t> FFmpegResample(uint8_t* srcData, size_t srcBytes, int srcChannels,
		int srcRate, int dstRate, AVSampleFormat srcSampleFmt, AVSampleFormat dstSampleFmt);

	// Manual resample, currently not used, because FFmpeg is faster:
	std::vector<float> ResampyResample(const std::vector<float>& srcData, int srcRate, int dstRate) const;
private:
	SwrContext* context_;
	uint8_t **srcData_, **dstData_;
};

#pragma warning(pop)