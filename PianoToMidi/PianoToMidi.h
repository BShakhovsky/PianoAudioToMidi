#pragma once
#include "FFmpegError.h"
#include "CqtError.h"
#include "MelError.h"
#include "KerasError.h"
#include "MidiOutError.h"

// namespace fdeep { class float_vec; }

class PianoToMidi
{
	static constexpr int nCqtBins = 3, rate = 16'000, nSeconds = 20;
	static constexpr float fMin = 30, fMax = 0;
	static constexpr bool htk = true;
	static constexpr const char *onsetsModel = "Magenta Onsets.json", *offsetsModel = "Magenta Offsets.json", *framesModel = "Magenta Frames.json", *volumesModel = "Magenta Volumes.json";
public:
	static constexpr int nMels = 229;

	PianoToMidi();
	~PianoToMidi();

	std::string FFmpegDecode(const char* fileName) const;

	std::string MelSpectrum() const;
	std::vector<float> GetMel() const;

	std::string CqtTotal() const;
	std::vector<float> GetCqt() const;
	size_t GetNumBins() const;
	size_t GetMidiSeconds() const;

	std::string HarmPerc() const;
	std::string Tempo() const;
	
	std::string KerasLoad(const std::string& currExePath) const;
	WPARAM RnnProbabs() const;

	const fdeep::float_vec& GetOnsets() const;
	const fdeep::float_vec& GetActives() const;
	const std::array<size_t, 8> & GetMelOctaves() const;
	const std::array<size_t, 88> & GetMelNoteIndices() const;

	std::string Gamma() const;
	std::string KeySignature() const;

	void WriteMidi(LPCTSTR fileName, std::string fileA) const;
private:
	std::vector<std::tuple<size_t, size_t, size_t, int>> CalcNoteIntervals() const;

	const std::unique_ptr<struct PianoData> data_;

	PianoToMidi(const PianoToMidi&) = delete;
	const PianoToMidi& operator=(const PianoToMidi&) = delete;
};