#include "stdafx.h"
#include "DirectSound.h"

using namespace std;
using namespace DirectX;

#pragma warning(push)
#pragma warning(disable: 26439) // This kind of function may not throw. Declare it 'noexcept' (f.6)
DirectSound::DirectSound()
{
	const auto hResult(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	if (FAILED(hResult)) throw SoundError("Could not initialize audio device");

	audio_ = make_unique<AudioEngine>(AudioEngine_Default
#ifdef _DEBUG
		| AudioEngine_Debug
#endif
		);
	if (!audio_->IsAudioDevicePresent()) throw SoundError("Could not find audio device");
}
#pragma warning(pop)

void DirectSound::Play(const WAVEFORMATEX* wavHeader, const uint8_t* rawData, size_t nBytes)
{
	unique_ptr<uint8_t[]>data(new uint8_t[nBytes]);
	CopyMemory(data.get(), rawData, nBytes);
	sound_ = make_unique<SoundEffect>(audio_.get(), data, wavHeader, rawData, nBytes);
	sound_->Play();
}