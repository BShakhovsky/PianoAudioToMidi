#include "stdafx.h"
#include "PianoToMidi.h"

#include "AudioLoader.h"
#include "AlignedVector.h"
#include "EnumFuncs.h"

#include "ConstantQ.h"
#include "HarmonicPercussive.h"
#include "Tempogram.h"
#include "KerasCnn.h"

using namespace std;
using namespace juce;

struct PianoData
{
	shared_ptr<AudioLoader> song;
	shared_ptr<ConstantQ> cqt;

	shared_ptr<HarmonicPercussive> hpss;
	vector<float> cqtHarmPadded;
	float bpm;

	unique_ptr<KerasCnn> cnn;
	vector<vector<float>> probabs;
	size_t index;

	vector<vector<pair<size_t, float>>> notes;
	vector<string> gamma;
	string keySign;

	PianoData() : bpm(0), index(0) {}
	~PianoData();

	MidiMessage GetKeySignEvent() const;
private:
	PianoData(const PianoData&) = delete;
	const PianoData& operator=(const PianoData&) = delete;
};
PianoData::~PianoData() {} // 4710 Function not inlined
MidiMessage PianoData::GetKeySignEvent() const
{
	if (keySign == "C" or keySign == "Am")
		return MidiMessage::keySignatureMetaEvent(0, keySign.back() == 'm');
	else if (keySign == "G" or keySign == "Dm")
		return MidiMessage::keySignatureMetaEvent(1, keySign.back() == 'm');
	else if (keySign == "D" or keySign == "Bm")
		return MidiMessage::keySignatureMetaEvent(2, keySign.back() == 'm');
	else if (keySign == "A" or keySign == "F#m")
		return MidiMessage::keySignatureMetaEvent(3, keySign.back() == 'm');
	else if (keySign == "E" or keySign == "C#m")
		return MidiMessage::keySignatureMetaEvent(4, keySign.back() == 'm');
	else if (keySign == "B" or keySign == "Abm")
		return MidiMessage::keySignatureMetaEvent(5, keySign.back() == 'm');

	else if (keySign == "F#" or keySign == "Ebm")
		return MidiMessage::keySignatureMetaEvent(6, keySign.back() == 'm');

	else if (keySign == "C#" or keySign == "Bbm")
		return MidiMessage::keySignatureMetaEvent(-5, keySign.back() == 'm');
	else if (keySign == "Ab" or keySign == "Fm")
		return MidiMessage::keySignatureMetaEvent(-4, keySign.back() == 'm');
	else if (keySign == "Eb" or keySign == "Cm")
		return MidiMessage::keySignatureMetaEvent(-3, keySign.back() == 'm');
	else if (keySign == "Bb" or keySign == "Gm")
		return MidiMessage::keySignatureMetaEvent(-2, keySign.back() == 'm');
	else if (keySign == "F" or keySign == "Dm")
		return MidiMessage::keySignatureMetaEvent(-1, keySign.back() == 'm');

	else
	{
		assert("Not all key signatures checked");
		return MidiMessage::keySignatureMetaEvent(0, false);
	}
}

PianoToMidi::PianoToMidi() : data_(make_unique<PianoData>()) {}
PianoToMidi::~PianoToMidi() {}

string PianoToMidi::FFmpegDecode(const char* mediaFile) const
{
	assert(not data_->song and "FFmpegDecode called twice");

	data_->song = make_shared<AudioLoader>(mediaFile);
	ostringstream os;
	os << "Format:\t\t" << data_->song->GetFormatName() << endl
		<< "Audio Codec:\t" << data_->song->GetCodecName() << endl
		<< "Bit_rate:\t\t" << data_->song->GetBitRate() << endl;

	data_->song->Decode();
//	os << "Duration:\t" << data_->song->GetNumSeconds() / 60 << " min : "
//		<< data_->song->GetNumSeconds() % 60 << " sec" << endl;
	data_->song->MonoResample();

	return move(os.str());
}

string PianoToMidi::CqtTotal() const
{
	assert(data_->song and "FFmpegDecode should be called before CqtTotal");
	assert(not data_->cqt and "CqtTotal called twice");
	
//	if (data_->song->GetBytesPerSample() == sizeof(uint16_t)) data_->song->MonoResample(0);
	assert(data_->song->GetBytesPerSample() == sizeof(float) and "Wrong raw audio data format");
	data_->cqt = make_shared<ConstantQ>(data_->song, 88 * nBins, 12 * nBins);

	data_->cqt->Amplitude2power();
	data_->cqt->TrimSilence();
	data_->cqt->Power2db(*min_element(data_->cqt->GetCQT().cbegin(), data_->cqt->GetCQT().cend()));

	assert(data_->cqt->GetCQT().size() % data_->cqt->GetNumBins() == 0
		and "Constant-Q spectrum is not rectangular");

	ostringstream os;
	os << "MIDI duration:\t" << GetMidiSeconds() / 60 << " min : "
		<< GetMidiSeconds() % 60 << " sec";
	return move(os.str());
}
vector<float> PianoToMidi::GetCqt() const
{
	vector<float> result;
	if (data_->cqt and not data_->cqt->GetCQT().empty())
		result.assign(data_->cqt->GetCQT().cbegin(), data_->cqt->GetCQT().cend());
	return move(result);
}
size_t PianoToMidi::GetNumBins() const { return data_->cqt->GetNumBins(); }
size_t PianoToMidi::GetMidiSeconds() const { return data_->cqt->GetCQT().size()
	/ data_->cqt->GetNumBins() * data_->cqt->GetHopLength() / data_->cqt->GetSampleRate(); }

string PianoToMidi::HarmPerc() const
{
	assert(data_->cqt and "CqtTotal should be called before HarmPerc");
	assert(not data_->hpss and "HarmPerc called twice");
	assert(data_->keySign.empty() and "Either HarmPerc called twice, or order is wrong");

	data_->hpss = make_shared<HarmonicPercussive>(data_->cqt);
	data_->cqtHarmPadded.assign(data_->hpss->GetHarmonic().size()
		+ (nFrames / 2 * 2) * data_->cqt->GetNumBins(), 0);
	const auto unusedIterFloat(copy(data_->hpss->GetHarmonic().cbegin(),
		data_->hpss->GetHarmonic().cend(), data_->cqtHarmPadded.begin()
		+ (nFrames / 2) * static_cast<ptrdiff_t>(data_->cqt->GetNumBins())));

	data_->hpss->OnsetEnvelope();
	data_->hpss->OnsetPeaksDetect();

	data_->hpss->Chromagram(false);
	data_->hpss->ChromaSum();

	data_->keySign = data_->hpss->KeySignature();
	ostringstream os;
	os << "Key signature:\tmaybe " << data_->keySign;
	return move(os.str());
}
string PianoToMidi::Tempo() const
{
	assert(data_->hpss and not data_->cqtHarmPadded.empty() and not data_->keySign.empty()
		and "HarmPerc should be called before Tempo");
	assert(data_->bpm == 0 and "Tempo called twice");

	Tempogram tempo;
//	data_->bpm = 0;
	data_->bpm = tempo.MostProbableTempo(data_->hpss->GetOnsetEnvelope(),
		data_->cqt->GetSampleRate(), data_->cqt->GetHopLength());

	ostringstream os;
	os << "Average tempo:\t";
	if (data_->bpm) os << round(data_->bpm);
	else
	{
		os << "don't know, audio is too short";
//		data_->bpm = 120;
	}
	return move(os.str());
}

string PianoToMidi::KerasLoad(const string& path) const
{
	assert(data_->hpss and not data_->keySign.empty()
		and "HarmPerc should be called before KerasLoad");
//	assert(data_->bpm and "Tempo should be called before KerasLoad");
	assert(not data_->cnn and "KerasLoad called twice");

	assert(data_->cqtHarmPadded.size() % data_->cqt->GetNumBins() == 0
		and "Harmonic spectrum is not rectangular");
	assert(data_->cqtHarmPadded.size() / data_->cqt->GetNumBins() >= nFrames
		and "Padded spectrum must contain at least nFrames time frames");

	data_->cnn = make_unique<KerasCnn>(path + "\\" + kerasModel);
	data_->probabs.resize(data_->cqtHarmPadded.size() / data_->cqt->GetNumBins() + 1 - nFrames);
	assert(data_->probabs.size() >= data_->hpss->GetOnsetPeaks().back()
		and "Input and output durations do not match");
//	data_->index = 0;

	return move(data_->cnn->GetLog());
}
WPARAM PianoToMidi::CnnProbabs() const
{
	assert(data_->cnn and "KerasLoad should be called before CnnProbabs");

#ifdef _DEBUG
	assert(data_->index == 0 and "CnnProbabs called wrong number of times");
	for (auto& timeFrame : data_->probabs)
	{
		timeFrame.resize(88);
		for (auto& p : timeFrame) p = .501f * rand() / RAND_MAX;
	}
	data_->index = data_->probabs.size() + 1;
	return 100;
#elif defined NDEBUG
	if (data_->index < data_->probabs.size())
	{
		data_->probabs.at(data_->index) = data_->cnn->Predict2D(
			data_->cqtHarmPadded.data() + static_cast<ptrdiff_t>(data_->index
				* data_->cqt->GetNumBins()), nFrames, data_->cqt->GetNumBins());
		return static_cast<WPARAM>(100. * data_->index++
			* data_->cqt->GetNumBins() / data_->cqtHarmPadded.size());
	}
	++data_->index;
	return 100;
#else
#pragma error Not debug, not release, then what is it?
#endif
}

string PianoToMidi::Gamma() const
{
	if (data_->index != data_->probabs.size() + 1)
		throw KerasError("CnnProbabs called wrong number of times");
	assert(data_->notes.empty() and data_->gamma.empty() and "Gamma called twice");

	vector<vector<float>> result(data_->hpss->GetOnsetPeaks().size(),
		vector<float>(data_->probabs.front().size()));
	for (size_t i(0); i < result.front().size(); ++i)
	{
		result.front().at(i) = max_element(data_->probabs.cbegin(),
			result.size() == 1 ? data_->probabs.cend() : (data_->probabs.cbegin()
				+ static_cast<ptrdiff_t>(data_->hpss->GetOnsetPeaks().front()
					+ data_->hpss->GetOnsetPeaks().at(1)) / 2),
			[i](const vector<float>& lhs, const vector<float>& rhs)
		{ return lhs.at(i) < rhs.at(i); })->at(i);
		if (result.size() > 1)
		{
			result.back().at(i) = max_element(data_->probabs.cbegin() + static_cast<ptrdiff_t>(
				*(data_->hpss->GetOnsetPeaks().cend() - 2)
				+ data_->hpss->GetOnsetPeaks().back()) / 2, data_->probabs.cend(),
				[i](const vector<float>& lhs, const vector<float>& rhs)
			{ return lhs.at(i) < rhs.at(i); })->at(i);

			for (size_t j(1); j < result.size() - 1; ++j) result.at(j).at(i) = max_element(
				data_->probabs.cbegin() + static_cast<ptrdiff_t>(
					data_->hpss->GetOnsetPeaks().at(j - 1) + data_->hpss->GetOnsetPeaks().at(j)) / 2,
				data_->probabs.cbegin() + static_cast<ptrdiff_t>(
					data_->hpss->GetOnsetPeaks().at(j) + data_->hpss->GetOnsetPeaks().at(j + 1)) / 2,
				[i](const vector<float>& lhs, const vector<float>& rhs)
			{ return lhs.at(i) < rhs.at(i); })->at(i);
		}
	}

	data_->notes.assign(result.size(), vector<pair<size_t, float>>());
	vector<pair<int, string>> notesCount(12);
	for (size_t i(0); i < notesCount.size(); ++i) notesCount.at(i) = make_pair(0,
		vector<string>{ "A", "Bb", "B", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab" }.at(i));

	for (size_t j(0); j < result.size(); ++j) for (size_t i(0); i < result.at(j).size(); ++i)
		if (result.at(j).at(i) > .5)
		{
			data_->notes.at(j).emplace_back(make_pair(i, result.at(j).at(i)));
			++notesCount.at(i % notesCount.size()).first;
		}
	sort(notesCount.rbegin(), notesCount.rend());

	data_->gamma.resize(7);
	const auto unusedIterStr(transform(notesCount.cbegin(), notesCount.cbegin()
		+ static_cast<ptrdiff_t>(data_->gamma.size()), data_->gamma.begin(),
#pragma warning(suppress:4710) // Function not inlined
		[](const pair<int, string>& val) { return val.second; }));

	ostringstream os;
	os << "Scale:\t\t";
	for (const auto& n : data_->gamma) os << n << ' ';
	return move(os.str());
}
string PianoToMidi::KeySignature() const
{
	assert(not data_->notes.empty() and not data_->gamma.empty()
		and "Gamma should be called before KeySignature");

	vector<string> blacks;
	for (const auto& n : data_->gamma) if (n.length() > 1) blacks.emplace_back(n);
	sort(blacks.begin(), blacks.end());

	string keySign;
	const auto MajorMinor([this](const string& mj, const string& mn)
	{
		return find(data_->gamma.cbegin(), data_->gamma.cend(), mj)
			- find(data_->gamma.cbegin(), data_->gamma.cend(), mn) < 0 ? mj : (mn + 'm');
	});

	if (blacks.empty()) keySign = MajorMinor("C", "A");
	else if (blacks.size() == 1)
	{
		if (blacks.front() == "F#")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "F") == data_->gamma.cend())
				keySign = MajorMinor("G", "E");
		}
		else if (blacks.front() == "Bb")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "B") == data_->gamma.cend())
				keySign = MajorMinor("F", "D");
		}
	}
	else if (blacks.size() == 2)
	{
		if (blacks.front() == "C#" and blacks.back() == "F#")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "C") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "F") == data_->gamma.cend())
				keySign = MajorMinor("D", "B");
		}
		else if (blacks.front() == "Bb" and blacks.back() == "Eb")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "B") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "E") == data_->gamma.cend())
				keySign = MajorMinor("Bb", "G");
		}
	}
	else if (blacks.size() == 3)
	{
		if (blacks.front() == "Ab" and blacks.at(1) == "C#" and blacks.back() == "F#")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "C") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "F") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "G") == data_->gamma.cend())
				keySign = MajorMinor("A", "F#");
		}
		else if (blacks.front() == "Ab" and blacks.at(1) == "Bb" and blacks.back() == "Eb")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "B") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "E") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "A") == data_->gamma.cend())
				keySign = MajorMinor("Eb", "C");
		}
	}
	else if (blacks.size() == 4)
	{
		if (blacks.front() == "Ab" and blacks.at(1) == "C#" and
			blacks.at(2) == "Eb" and blacks.back() == "F#")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "C") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "D") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "F") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "G") == data_->gamma.cend())
				keySign = MajorMinor("E", "C#");
		}
		if (blacks.front() == "Ab" and blacks.at(1) == "Bb" and
			blacks.at(2) == "C#" and blacks.back() == "Eb")
		{
			if (find(data_->gamma.cbegin(), data_->gamma.cend(), "B") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "E") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "A") == data_->gamma.cend() and
				find(data_->gamma.cbegin(), data_->gamma.cend(), "D") == data_->gamma.cend())
				keySign = MajorMinor("Ab", "F");
		}
	}
	else if (find(data_->gamma.cbegin(), data_->gamma.cend(), "B") != data_->gamma.cend()
		and find(data_->gamma.cbegin(), data_->gamma.cend(), "E") != data_->gamma.cend())
		keySign = MajorMinor("B", "Ab");
	else if (find(data_->gamma.cbegin(), data_->gamma.cend(), "C") != data_->gamma.cend()
		and find(data_->gamma.cbegin(), data_->gamma.cend(), "F") != data_->gamma.cend())
		keySign = MajorMinor("C#", "Bb");

	if (not keySign.empty()) data_->keySign = keySign;
	return "Key signature:\t" + (keySign.empty() ? "The scale does not correspond to any" : keySign);
}

void PianoToMidi::WriteMidi(const char* midiFile) const
{
	using boost::filesystem::exists;

	assert(not data_->notes.empty() and "Gamma should be called before WriteMidi");
	assert(data_->notes.size() == data_->hpss->GetOnsetPeaks().size()
		and "Wrong number of note onsets");

	const auto outputFile(File::getCurrentWorkingDirectory().getChildFile(String(midiFile)));
	if (exists(outputFile.getFullPathName().toStdString()) and not outputFile.deleteFile())
		throw MidiOutError((string("Could not delete MIDI file: ") + midiFile).c_str());
	FileOutputStream outputStream(outputFile.getFullPathName());
	if (outputStream.failedToOpen())
		throw MidiOutError((string("Could not open MIDI file: ") + midiFile).c_str());
	
	MidiMessageSequence track;
	track.addEvent(MidiMessage::textMetaEvent(1, "Automatically transcribed from audio"));
	track.addEvent(MidiMessage::textMetaEvent(2, "Used software created by Boris Shakhovsky"));
	track.addEvent(data_->GetKeySignEvent());

	MidiFile midi;
	constexpr auto ppqn(480);
	midi.setTicksPerQuarterNote(ppqn);
	const auto tempoEvent(MidiMessage::tempoMetaEvent(
		static_cast<int>(round(1'000'000 * 60 / data_->bpm))));

	for (const auto& note : data_->notes.back())
		track.addEvent(MidiMessage::noteOn(1, static_cast<int>(note.first) + 21, note.second));
	for (size_t i(data_->notes.size() - 1); i > 0; --i)
	{
		track.addTimeToMessages(static_cast<double>(
			data_->hpss->GetOnsetPeaks().at(i) - data_->hpss->GetOnsetPeaks().at(i - 1)) // delta frame
			* data_->cqt->GetHopLength() / data_->cqt->GetSampleRate() // delta frame to delta seconds
			// multiply by pulses per seconds (ppqn * tempo / 60):
			/ tempoEvent.getTempoMetaEventTickLength(midi.getTimeFormat()));
		for (const auto& note : data_->notes.at(i))
			track.addEvent(MidiMessage::noteOff(1, static_cast<int>(note.first) + 21));
		for (const auto& note : data_->notes.at(i - 1))
			track.addEvent(MidiMessage::noteOn(1, static_cast<int>(note.first) + 21, note.second));
	}
	track.addEvent(tempoEvent);
	track.updateMatchedPairs();

	midi.addTrack(track);
	if (not midi.writeTo(outputStream))
		throw MidiOutError((string("Could not write to MIDI file: ") + midiFile).c_str());
}