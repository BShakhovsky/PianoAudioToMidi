#include "stdafx.h"
#include "PianoToMidi.h"

#include "AudioLoader.h"
#include "AlignedVector.h"
#include "EnumTypes.h"
#include "ConstantQ.h"
#include "HarmonicPercussive.h"
#include "KerasCnn.h"

using namespace std;

struct PianoData
{
	shared_ptr<AudioLoader> song;
	shared_ptr<ConstantQ> cqt;

	vector<float> cqtHarm;
	vector<size_t> oPeaks;

	unique_ptr<KerasCnn> cnn;
	vector<vector<float>> probabs;
	size_t index;

	vector<vector<pair<size_t, float>>> notes;
	vector<string> gamma;
	string keySign;

	PianoData() : index(0) {}
	~PianoData();
private:
	PianoData(const PianoData&) = delete;
	const PianoData& operator=(const PianoData&) = delete;
};
PianoData::~PianoData() {} // 4710 Function not inlined

PianoToMidi::PianoToMidi() : data_(make_unique<PianoData>()) {}
PianoToMidi::~PianoToMidi() {}

string PianoToMidi::FFmpegDecode(const char* mediaFile) const
{
	assert(not data_->song and "FFmpegDecode called twice");

	data_->song = make_shared<AudioLoader>(mediaFile);
	ostringstream os;
	os << "Format:\t\t" << data_->song->GetFormatName() << endl
		<< "Audio Codec:\t" << data_->song->GetCodecName() << endl
		<< "Bit_rate:\t" << data_->song->GetBitRate() << endl;

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
	const auto nSecs(data_->cqt->GetCQT().size() / data_->cqt->GetNumBins()
		* data_->cqt->GetHopLength() / data_->cqt->GetSampleRate());

	ostringstream os;
	os << "MIDI duration:\t" << nSecs / 60 << " min : " << nSecs % 60 << " sec";
	return move(os.str());
}
string PianoToMidi::HarmPerc() const
{
	assert(data_->cqt and "CqtTotal should be called before HarmPerc");
	assert(data_->cqtHarm.empty() and "HarmPerc called twice");
	assert(data_->keySign.empty() and "Either HarmPerc called twice, or order is wrong");

	HarmonicPercussive hpss(data_->cqt);
	data_->cqtHarm.assign(hpss.GetHarmonic().size() + (nFrames / 2 * 2) * data_->cqt->GetNumBins(), 0);
	const auto unusedIterFloat(copy(hpss.GetHarmonic().cbegin(), hpss.GetHarmonic().cend(),
		data_->cqtHarm.begin() + (nFrames / 2) * static_cast<ptrdiff_t>(data_->cqt->GetNumBins())));

	hpss.OnsetEnvelope();
	hpss.OnsetPeaksDetect();
	data_->oPeaks = hpss.GetOnsetPeaks();

	hpss.Chromagram(false);
	hpss.ChromaSum();

	data_->keySign = hpss.KeySignature();
	ostringstream os;
	os << "Key signature:\tmaybe " << data_->keySign;
	return move(os.str());
}

string PianoToMidi::KerasLoad() const
{
	assert(not data_->cqtHarm.empty() and not data_->keySign.empty()
		and "HarmPerc should be called before KerasLoad");
	assert(not data_->cnn and "KerasLoad called twice");

	assert(data_->cqtHarm.size() % data_->cqt->GetNumBins() == 0
		and "Harmonic spectrum is not rectangular");
	assert(data_->cqtHarm.size() / data_->cqt->GetNumBins() >= nFrames
		and "Padded spectrum must contain at least nFrames time frames");

	data_->cnn = make_unique<KerasCnn>(kerasModel);
	data_->probabs.resize(data_->cqtHarm.size() / data_->cqt->GetNumBins() + 1 - nFrames);
	assert(data_->probabs.size() >= data_->oPeaks.back() and "Input and output durations do not match");
//	data_->index = 0;

	return move(data_->cnn->GetLog());
}
int PianoToMidi::CnnProbabs() const
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
			data_->cqtHarm.data() + static_cast<ptrdiff_t>(data_->index
				* data_->cqt->GetNumBins()), nFrames, data_->cqt->GetNumBins());
		return static_cast<int>(100. * data_->index++ * data_->cqt->GetNumBins() / data_->cqtHarm.size());
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

	vector<vector<float>> result(data_->oPeaks.size(), vector<float>(data_->probabs.front().size()));
	for (size_t i(0); i < result.front().size(); ++i)
	{
		result.front().at(i) = max_element(data_->probabs.cbegin(),
			result.size() == 1 ? data_->probabs.cend() : (data_->probabs.cbegin()
				+ static_cast<ptrdiff_t>(data_->oPeaks.front() + data_->oPeaks.at(1)) / 2),
			[i](const vector<float>& lhs, const vector<float>& rhs)
		{ return lhs.at(i) < rhs.at(i); })->at(i);
		if (result.size() > 1)
		{
			result.back().at(i) = max_element(data_->probabs.cbegin() + static_cast<ptrdiff_t>(
				*(data_->oPeaks.cend() - 2) + data_->oPeaks.back()) / 2, data_->probabs.cend(),
				[i](const vector<float>& lhs, const vector<float>& rhs)
			{ return lhs.at(i) < rhs.at(i); })->at(i);

			for (size_t j(1); j < result.size() - 1; ++j) result.at(j).at(i) = max_element(
				data_->probabs.cbegin() + static_cast<ptrdiff_t>(
					data_->oPeaks.at(j - 1) + data_->oPeaks.at(j)) / 2,
				data_->probabs.cbegin() + static_cast<ptrdiff_t>(
					data_->oPeaks.at(j) + data_->oPeaks.at(j + 1)) / 2,
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
	os << "Scale:\t";
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
	using namespace juce;

	assert(not data_->notes.empty() and "Gamma should be called before WriteMidi");
	assert(data_->notes.size() == data_->oPeaks.size() and "Wrong number of note onsets");

	const auto outputFile(File::getCurrentWorkingDirectory().getChildFile(String(midiFile)));
	if (exists(outputFile.getFullPathName().toStdString()) and not outputFile.deleteFile())
		throw MidiOutError((string("Could not delete MIDI file: ") + midiFile).c_str());
	FileOutputStream outputStream(outputFile.getFullPathName());
	if (outputStream.failedToOpen())
		throw MidiOutError((string("Could not open MIDI file: ") + midiFile).c_str());
	
	MidiMessageSequence track;
	track.addEvent(MidiMessage::textMetaEvent(1, "Automatically transcribed from audio"));
	track.addEvent(MidiMessage::textMetaEvent(2, "Used software created by Boris Shakhovsky"));

	if (data_->keySign == "C" or data_->keySign == "Am")
		track.addEvent(MidiMessage::keySignatureMetaEvent(0, data_->keySign.back() == 'm'));
	else if (data_->keySign == "G" or data_->keySign == "Dm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(1, data_->keySign.back() == 'm'));
	else if (data_->keySign == "D" or data_->keySign == "Bm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(2, data_->keySign.back() == 'm'));
	else if (data_->keySign == "A" or data_->keySign == "F#m")
		track.addEvent(MidiMessage::keySignatureMetaEvent(3, data_->keySign.back() == 'm'));
	else if (data_->keySign == "E" or data_->keySign == "C#m")
		track.addEvent(MidiMessage::keySignatureMetaEvent(4, data_->keySign.back() == 'm'));
	else if (data_->keySign == "B" or data_->keySign == "Abm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(5, data_->keySign.back() == 'm'));

	else if (data_->keySign == "F#" or data_->keySign == "Ebm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(6, data_->keySign.back() == 'm'));

	else if (data_->keySign == "C#" or data_->keySign == "Bbm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(-5, data_->keySign.back() == 'm'));
	else if (data_->keySign == "Ab" or data_->keySign == "Fm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(-4, data_->keySign.back() == 'm'));
	else if (data_->keySign == "Eb" or data_->keySign == "Cm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(-3, data_->keySign.back() == 'm'));
	else if (data_->keySign == "Bb" or data_->keySign == "Gm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(-2, data_->keySign.back() == 'm'));
	else if (data_->keySign == "F" or data_->keySign == "Dm")
		track.addEvent(MidiMessage::keySignatureMetaEvent(-1, data_->keySign.back() == 'm'));

	else assert("Not all key signatures checked");


	MidiFile midi;
	constexpr auto ppqn(480);
	midi.setTicksPerQuarterNote(ppqn);

	//	microSecPerBeat = mido.bpm2tempo(lbr.beat.tempo(song).mean());
	constexpr auto tempo(120);
	const auto tempoEvent(MidiMessage::tempoMetaEvent(1'000'000 * 60 / tempo));

	for (const auto& note : data_->notes.back())
		track.addEvent(MidiMessage::noteOn(1, static_cast<int>(note.first) + 21, note.second));
	for (size_t i(data_->notes.size() - 1); i > 0; --i)
	{
		track.addTimeToMessages(static_cast<double>(
			data_->oPeaks.at(i) - data_->oPeaks.at(i - 1)) // delta frame
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