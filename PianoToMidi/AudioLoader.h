#pragma once
#include "FFmpegError.h"

class AudioLoader
{
public:
	explicit AudioLoader(const char* fileName);
	void Decode() const;
	~AudioLoader();

	const char* GetFormatName() const;
	const char* GetCodecName() const;
	int64_t GetBitRate() const;

	const std::vector<uint8_t>& GetRawData() const;
	size_t GetNumSeconds() const;

	void MonoResample(int rate = 22'050, AVSampleFormat format = AV_SAMPLE_FMT_FLT) const;
private:
	void FindAudioStream() const;
	int DecodePacket() const;

	const std::unique_ptr<struct FFmpegData> data_;

	AudioLoader(const AudioLoader&) = delete;
	AudioLoader operator=(const AudioLoader&) = delete;
};