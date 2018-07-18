#pragma once
#include "SoundError.h"

class DirectSound
{
public:
	DirectSound();
#pragma warning(push)
#pragma warning(disable: 4514)	// Unreferenced inline function has been removed
	~DirectSound() { if (audio_) audio_->Suspend(); }
#pragma warning(pop)
	void Play(const WAVEFORMATEX* wavHeader, const uint8_t* rawData, size_t nBytes);
private:
	std::unique_ptr<DirectX::AudioEngine> audio_;
	std::unique_ptr<DirectX::SoundEffect> sound_;

	DirectSound(const DirectSound&) = delete;
	DirectSound operator=(const DirectSound&) = delete;
};