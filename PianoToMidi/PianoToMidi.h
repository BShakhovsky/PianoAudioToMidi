#pragma once
#include "FFmpegError.h"
#include "CqtError.h"
#include "KerasError.h"
#include "MidiOutError.h"

class PianoToMidi
{
	static constexpr auto nBins = 4, nFrames = 7;
	static constexpr auto kerasModel = "KerasModel Dixon61 Frame54.json";
public:
	PianoToMidi();
	~PianoToMidi();

	std::string FFmpegDecode(const char* fileName) const;

	std::string CqtTotal() const;
	std::vector<float> GetCqt() const;
	size_t GetNumBins() const;
	size_t GetMidiSeconds() const;

	std::string HarmPerc() const;
	std::string Tempo() const;
	
	std::string KerasLoad(const std::string& currExePath) const;
	WPARAM CnnProbabs() const;

	std::string Gamma() const;
	const std::vector<size_t>& GetOnsetFrames() const;
	const std::vector<std::vector<std::pair<size_t, float>>>& GetNotes() const;
	std::string KeySignature() const;

	void WriteMidi(const char* fileName) const;
private:
	const std::unique_ptr<struct PianoData> data_;

	PianoToMidi(const PianoToMidi&) = delete;
	const PianoToMidi& operator=(const PianoToMidi&) = delete;
};