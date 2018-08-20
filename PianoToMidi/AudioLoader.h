#pragma once

class AudioLoader
{
public:
	explicit AudioLoader(const char* fileName);
	void Decode() const;
	~AudioLoader();

	const char* GetFormatName() const;
	const char* GetCodecName() const;
	int64_t GetBitRate() const;

	uint8_t* GetRawData(); // not const to allow data to be overwritten from outside
	size_t GetNumBytes() const;
	int GetBytesPerSample() const;
	size_t GetNumSamples() const;
	int GetSampleRate() const;
	size_t GetNumSeconds() const;

	// If not float, then S16 format for playing with DirectSound:
	void MonoResample(int rate = 22'050, bool isFloatFormat = true) const;
private:
	void FindAudioStream() const;
	int DecodePacket() const;

	const std::unique_ptr<struct FFmpegData> data_;

	AudioLoader(const AudioLoader&) = delete;
	const AudioLoader& operator=(const AudioLoader&) = delete;
};