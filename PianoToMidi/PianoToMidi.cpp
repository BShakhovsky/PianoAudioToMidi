#include "stdafx.h"
#include "PianoToMidi.h"

#include "AudioLoader.h"
#include "AlignedVector.h"
#include "EnumFuncs.h"

#include "MelTransform.h"
#include "ConstantQ.h"
#include "SpecPostProc.h"

#include "HarmonicPercussive.h"
#include "Tempogram.h"

#include "KerasRnn.h"

using namespace std;
using namespace juce;
using fdeep::float_vec;

struct PianoData
{
	shared_ptr<AudioLoader> song;
	shared_ptr<MelTransform> mel;
	shared_ptr<ConstantQ> cqt;

	shared_ptr<HarmonicPercussive> hpss;
	float bpm;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#endif

	unique_ptr<KerasRnn> onsets, offsets, frames, volumes;
	float_vec melPadded, onsetProbs, offsetProbs, frameProbs, volumeProbs;
	size_t nFrames, index;

	vector<array<int, 88>> pianoRoll;
	vector<string> gamma;
	string keySign;

	PianoData() : bpm(0), nFrames(0), index(0) {}
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
	else if (keySign == "G" or keySign == "Em")
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
	os << "Duration:\t" << data_->song->GetNumSeconds() / 60 << " min : "
		<< data_->song->GetNumSeconds() % 60 << " sec" << endl;
	data_->song->MonoResample(rate);

	return move(os.str());
}

string PianoToMidi::MelSpectrum() const
{
	assert(not data_->mel and "Mel transform calculated twice");
	data_->mel = make_unique<MelTransform>(data_->song, rate, nMels, fMin, fMax, htk);

	SpecPostProc::TrimSilence(data_->mel->GetMel().get(), nMels);
	SpecPostProc::Power2db(data_->mel->GetMel().get());

	data_->mel->CalcOctaveIndices();

	return data_->mel->GetLog() + "Log mel-scaled spectrogram calculated";
}
string PianoToMidi::CqtTotal() const
{
	assert(data_->song and "FFmpegDecode should be called before CqtTotal");
	assert(not data_->cqt and "CqtTotal called twice");
	
//	if (data_->song->GetBytesPerSample() == sizeof(uint16_t)) data_->song->MonoResample(0);
	assert(data_->song->GetBytesPerSample() == sizeof(float) and "Wrong raw audio data format");
	data_->cqt = make_shared<ConstantQ>(data_->song, 88 * nCqtBins, 12 * nCqtBins);

	data_->song.reset();

	ostringstream os;
	os << "Constant-Q spectrogram calculated" << endl << endl;

	SpecPostProc::Amplitude2power(data_->cqt->GetCQT().get());
	SpecPostProc::TrimSilence(data_->cqt->GetCQT().get(), data_->cqt->GetNumBins());
	SpecPostProc::Power2db(data_->cqt->GetCQT().get(), *min_element(data_->cqt->GetCQT()->cbegin(), data_->cqt->GetCQT()->cend()));

	assert(data_->cqt->GetCQT()->size() % data_->cqt->GetNumBins() == 0
		and "Constant-Q spectrum is not rectangular");

	os << "MIDI duration:\t" << GetMidiSeconds() / 60 << " min : "
		<< GetMidiSeconds() % 60 << " sec";
	return move(os.str());
}

#define GET_SPECTRUM(NAME, DATA, FUNC) vector<float> PianoToMidi::Get##NAME##() const { vector<float> result; if (data_->##DATA and not data_->##DATA##->Get##FUNC##()->empty()) \
	result.assign(data_->##DATA##->Get##FUNC##()->cbegin(), data_->##DATA##->Get##FUNC##()->cend()); return /*move(*/result; }
GET_SPECTRUM(Cqt, cqt, CQT)
GET_SPECTRUM(Mel, mel, Mel)

size_t PianoToMidi::GetNumBins() const { return data_->cqt->GetNumBins(); }
size_t PianoToMidi::GetMidiSeconds() const
{
	assert(rate == data_->cqt->GetSampleRate() and "Different sample rates for Mel & Cqt transforms");
	const auto nMelSamples(data_->mel->GetMel()->size() / nMels * data_->mel->GetHopLen()),
		nCqtSamples(data_->cqt->GetCQT()->size() / data_->cqt->GetNumBins() * data_->cqt->GetHopLength());
	data_->mel->GetMel()->resize(min(nMelSamples, nCqtSamples) / data_->mel->GetHopLen() * nMels);
	data_->cqt->GetCQT()->resize(min(nMelSamples, nCqtSamples) / data_->cqt->GetHopLength() * data_->cqt->GetNumBins());
	
	const auto nMelSecs(nMelSamples / rate), nCqtSecs(nCqtSamples / data_->cqt->GetSampleRate());
	assert(nMelSecs == nCqtSecs and "Different seconds duration for Mel & Cqt transforms");
	return min(nMelSecs, nCqtSecs);
}

string PianoToMidi::HarmPerc() const
{
	assert(data_->cqt and "CqtTotal should be called before HarmPerc");
	assert(not data_->hpss and "HarmPerc called twice");
	assert(data_->keySign.empty() and "Either HarmPerc called twice, or order is wrong");

	data_->hpss = make_shared<HarmonicPercussive>(data_->cqt);

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
	assert(data_->hpss and not data_->keySign.empty()
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
	data_->hpss.reset();

//	assert(data_->bpm and "Tempo should be called before KerasLoad");
	assert(not data_->onsets and not data_->offsets and not data_->frames and not data_->volumes and "KerasLoad called twice");

	assert(data_->nFrames == 0 and "Number of frames calculated twice");
	data_->nFrames = static_cast<size_t>(nSeconds) * rate / data_->mel->GetHopLen() + 1;
	data_->melPadded.resize(((data_->mel->GetMel()->size() / nMels - 1) / data_->nFrames + 1) * data_->nFrames * nMels);
	copy(data_->mel->GetMel()->cbegin(), data_->mel->GetMel()->cend(), data_->melPadded.begin());
	fill(data_->melPadded.begin() + static_cast<ptrdiff_t>(data_->mel->GetMel()->size()), data_->melPadded.end(), *min_element(data_->mel->GetMel()->cbegin(), data_->mel->GetMel()->cend()));

#ifdef _DEBUG
	UNREFERENCED_PARAMETER(path);
#elif defined NDEBUG
	data_->onsets	= make_unique<KerasRnn>(path + "\\" + onsetsModel);
	data_->offsets	= make_unique<KerasRnn>(path + "\\" + offsetsModel);
	data_->frames	= make_unique<KerasRnn>(path + "\\" + framesModel);
	data_->volumes	= make_unique<KerasRnn>(path + "\\" + volumesModel);
#else
#pragma error Not debug, not release, then what is it?
#endif

	data_->onsetProbs .resize(data_->melPadded.size() / nMels * 88);
	data_->offsetProbs.resize(data_->melPadded.size() / nMels * 88);
	data_->frameProbs .resize(data_->melPadded.size() / nMels * 88);
	data_->volumeProbs.resize(data_->melPadded.size() / nMels * 88);
//	data_->index = 0;

#ifdef _DEBUG
	return "";
#elif defined NDEBUG
	return move(data_->onsets->GetLog() + data_->offsets->GetLog() + data_->frames->GetLog() + data_->volumes->GetLog());
#else
#pragma error Not debug, not release, then what is it?
#endif
}
WPARAM PianoToMidi::RnnProbabs() const
{
//	assert(data_->onsets and data_->offsets and data_->frames and data_->volumes and "KerasLoad should be called before RnnProbabs");

	if (data_->index / 4 <= (data_->onsetProbs.size() - 1) / data_->nFrames / 88)
	{
#ifdef _DEBUG
		for (size_t i(0); i < data_->nFrames * 88; ++i)
		{
			data_-> onsetProbs.at(i + data_->index / 4 * data_->nFrames * 88) = static_cast<float>(.501 * rand() / RAND_MAX);
			data_->offsetProbs.at(i + data_->index / 4 * data_->nFrames * 88) = static_cast<float>(.501 * rand() / RAND_MAX);
			data_-> frameProbs.at(i + data_->index / 4 * data_->nFrames * 88) = static_cast<float>(.501 * rand() / RAND_MAX);
			data_->volumeProbs.at(i + data_->index / 4 * data_->nFrames * 88) = static_cast<float>(.501 * rand() / RAND_MAX);
		}
		Sleep(500);
#elif defined NDEBUG
		switch (data_->index % 4)
		{
		case 0:
		{
			const auto onProb(data_->onsets->Predict2D(data_->melPadded.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * nMels), data_->nFrames, nMels));
			copy(onProb.cbegin(), onProb.cend(), data_->onsetProbs.begin() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88));
		} break;
		case 1:
		{
			const auto offProb(data_->offsets->Predict2D(data_->melPadded.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * nMels), data_->nFrames, nMels));
			copy(offProb.cbegin(), offProb.cend(), data_->offsetProbs.begin() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88));
		} break;
		case 2:
		{
			const auto frProb(data_->frames->PredictMulti(data_->melPadded.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * nMels), data_->nFrames, nMels,
				data_->onsetProbs.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88),
				data_->offsetProbs.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88), 88));
			copy(frProb.cbegin(), frProb.cend(), data_->frameProbs.begin() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88));
		} break;
		case 3:
		{
			const auto volProb(data_->volumes->Predict2D(data_->melPadded.data() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * nMels), data_->nFrames, nMels));
			copy(volProb.cbegin(), volProb.cend(), data_->volumeProbs.begin() + static_cast<ptrdiff_t>(data_->index / 4 * data_->nFrames * 88));
		} break;
		default: assert(not "Remainder of division operation is somehow wrong");
		}
#else
#pragma error Not debug, not release, then what is it?
#endif
		return 100 * ++data_->index / 4 / ((data_->onsetProbs.size() - 1) / data_->nFrames / 88 + 1);
	}
	return 100;
}

const float_vec& PianoToMidi::GetOnsets() const { return data_->onsetProbs; }
const float_vec& PianoToMidi::GetActives() const { return data_->frameProbs; }
const array<size_t, 8>& PianoToMidi::GetMelOctaves() const { return data_->mel->GetOctaveIndices(); }
const array<size_t, 88>& PianoToMidi::GetMelNoteIndices() const { return data_->mel->GetNoteIndices(); }

vector<tuple<size_t, size_t, size_t, int>> PianoToMidi::CalcNoteIntervals() const
{
	// Ensure that any frame with an onset prediction is considered active:
	transform(data_->onsetProbs.cbegin(), data_->onsetProbs.cend(), data_->frameProbs.cbegin(),
		data_->frameProbs.begin(), [](const float on, const float fr) { return (on > .5 or fr > .5) ? 1.f : 0.f; });

	vector<tuple<size_t, size_t, size_t, int>> result;

	array<int, 88> starts;
	fill(starts.begin(), starts.end(), -1);

	const auto EndPitch([this, &result, &starts](size_t pitch, size_t endFrame)
		{
			result.emplace_back(make_tuple(pitch, starts.at(pitch), endFrame, static_cast<int>(data_->volumeProbs.at(starts.at(pitch) *
#ifdef _WIN64
				88ull
#else
				88ul
#endif
				+ pitch) * 80 + 10)));
			starts.at(pitch) = -1;
		});

	// Add silent frame at the end so we can do a final loop and terminate any notes that are still active:
	data_->frameProbs.resize(data_->frameProbs.size() + starts.size());
	fill(data_->frameProbs.end() - static_cast<ptrdiff_t>(starts.size()), data_->frameProbs.end(), 0);
	for (size_t i(0); i < data_->frameProbs.size() / starts.size(); ++i) for (size_t pitch(0); pitch < starts.size(); ++pitch)
		if (data_->frameProbs.at(i * starts.size() + pitch))
		{
			if (starts.at(pitch) == -1)
			{
				if (data_->onsetProbs.at(i * starts.size() + pitch) > .5) starts.at(pitch) = static_cast<int>(i); // Start a note only if we have predicted an onset
				// else; // Even though the frame is active, there is no onset, so ignore it
			}
			else if (data_->onsetProbs.at(i * starts.size() + pitch) > .5 and data_->onsetProbs.at((i - 1) * starts.size() + pitch) < .5)
			{
				EndPitch(pitch, i);						// Pitch is already active, but because of a new onset, we should end the note
				starts.at(pitch) = static_cast<int>(i);	// and start a new one
			}
		}
		else if (starts.at(pitch) != -1) EndPitch(pitch, i);

	assert(data_->offsetProbs.empty() and "Offsets should have already been released");
	data_->volumeProbs.clear();

	data_->pianoRoll.resize(data_->frameProbs.size() / starts.size());
	return /*move(*/result;
}
string PianoToMidi::Gamma() const
{
	data_->onsets.reset();
	data_->offsets.reset();
	data_->frames.reset();
	data_->volumes.reset();
	data_->melPadded.clear();

	data_-> onsetProbs.resize(data_->mel->GetMel()->size() / nMels * 88);
	data_->offsetProbs.clear();
	data_-> frameProbs.resize(data_->mel->GetMel()->size() / nMels * 88);
	data_->volumeProbs.resize(data_->mel->GetMel()->size() / nMels * 88);

	if (data_->index % 4 or data_->index / 4 != (data_->onsetProbs.size() - 1) / data_->nFrames / 88 + 1)
		throw KerasError("RnnProbabs called wrong number of times");
	assert(data_->pianoRoll.empty() and data_->gamma.empty() and "Gamma called twice");

	array<pair<int, string>, 12> notesCount;
	for (size_t i(0); i < notesCount.size(); ++i) notesCount.at(i).second = vector<string>{
		"A", "Bb", "B", "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab" }.at(i);

	assert(data_->pianoRoll.empty() and "Gamma have been called twice");
	const auto intervals(CalcNoteIntervals());
	for (const auto& n : intervals)
	{
		data_->pianoRoll.at(get<1>(n)).at(get<0>(n)) = get<3>(n);
		data_->pianoRoll.at(get<2>(n)).at(get<0>(n)) = -1;
		++notesCount.at(get<0>(n) % notesCount.size()).first;
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
	assert(not data_->pianoRoll.empty() and not data_->gamma.empty()
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

void PianoToMidi::WriteMidi(LPCTSTR midiFile, string fileA) const
{
	using placeholders::_1;
	using boost::filesystem::exists;

	assert(not data_->pianoRoll.empty() and "Gamma should be called before WriteMidi");
//	assert(data_->pianoRoll.size() == data_->hpss->GetOnsetPeaks().size() and "Wrong number of note onsets");

	const auto outputFile(File::getCurrentWorkingDirectory().getChildFile(String(midiFile)));
	if (exists(outputFile.getFullPathName().toStdString()) and not outputFile.deleteFile())
		throw MidiOutError(("Could not delete MIDI file: " + fileA).c_str());
	FileOutputStream outputStream(outputFile.getFullPathName());
	if (outputStream.failedToOpen())
		throw MidiOutError(("Could not open MIDI file: " + fileA).c_str());

	MidiMessageSequence track;
	track.addEvent(MidiMessage::textMetaEvent(1, "Automatically transcribed from audio"));
	track.addEvent(MidiMessage::textMetaEvent(2, "Used Windows App created by Boris Shakhovsky"));
	track.addEvent(MidiMessage::textMetaEvent(3, "Acoustic Grand Piano"));
	track.addEvent(data_->GetKeySignEvent());

	MidiFile midi;
	constexpr auto ppqn(480);
	midi.setTicksPerQuarterNote(ppqn);
	const auto tempoEvent(MidiMessage::tempoMetaEvent(
		static_cast<int>(round(1'000'000 * 60 / data_->bpm))));

	assert(not data_->pianoRoll.empty() and "Piano roll should be called before WriteMidi");
	size_t iRight(data_->pianoRoll.size() - 1);
	for (; iRight and all_of(data_->pianoRoll.at(iRight).cbegin(), data_->pianoRoll.at(iRight).cend(), bind(equal_to<int>(), 0, _1)); --iRight);
	if (iRight == 0) throw MidiOutError("There are no notes, nothing to write to MIDI");

	for (size_t j(0); j < data_->pianoRoll.at(iRight).size(); ++j)
		if (data_->pianoRoll.at(iRight).at(j) > 0) track.addEvent(MidiMessage::noteOn(1, static_cast<int>(j) + 21, static_cast<uint8>(data_->pianoRoll.at(iRight).at(j))));
		else if (data_->pianoRoll.at(iRight).at(j) == -1) track.addEvent(MidiMessage::noteOff(1, static_cast<int>(j) + 21));
		else assert(data_->pianoRoll.at(iRight).at(j) == 0 and "Wrong piano roll value");

	for (size_t i(iRight); i; --i)
	{
		if (all_of(data_->pianoRoll.at(i - 1).cbegin(), data_->pianoRoll.at(i - 1).cend(), bind(equal_to<int>(), 0, _1))) continue;

		track.addTimeToMessages((iRight - i + 1.) // delta frame
			* data_->mel->GetHopLen() / rate // delta frame to delta seconds
			// multiply by pulses per seconds (ppqn * tempo / 60):
			/ tempoEvent.getTempoMetaEventTickLength(midi.getTimeFormat()));
		for (size_t j(0); j < data_->pianoRoll.at(i - 1).size(); ++j)
			if (data_->pianoRoll.at(i - 1).at(j) > 0) track.addEvent(MidiMessage::noteOn(1, static_cast<int>(j) + 21, static_cast<uint8>(data_->pianoRoll.at(i - 1).at(j))));
			else if (data_->pianoRoll.at(i - 1).at(j) == -1) track.addEvent(MidiMessage::noteOff(1, static_cast<int>(j) + 21));
			else assert(data_->pianoRoll.at(i - 1).at(j) == 0 and "Wrong piano roll value");

		iRight = i - 1;
	}
	track.addEvent(tempoEvent);
	track.updateMatchedPairs();

	midi.addTrack(track);
	data_->pianoRoll.clear();
	if (not midi.writeTo(outputStream))
		throw MidiOutError(("Could not write to MIDI file: " + fileA).c_str());
}