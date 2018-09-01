#include "stdafx.h"
#include "PianoToMidi_Win.h"
#include "PianoToMidi.h"
#include "CursorWait.h"
#include "CanvasGdi.h"
#include "BitmapCompatible.h"

using namespace std;

LONG GetWindowWidth(const HWND hWnd)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	return rect.right - rect.left;
}
LONG GetWindowHeight(const HWND hWnd)
{
	RECT rect;
	GetWindowRect(hWnd, &rect);
	return rect.bottom - rect.top;
}

PianoToMidi_Win::PianoToMidi_Win(const HWND hDlg, const int calcSpectr, const int spectrTitle,
	const int convert, const int progBar, const int log, const int spectr)
	: media_(make_unique<PianoToMidi>()),
	hDlg_(hDlg),
	calcSpectr_(GetDlgItem(hDlg, calcSpectr)),
	spectrTitle_(GetDlgItem(hDlg, spectrTitle)),
	spectr_(GetDlgItem(hDlg, spectr)),
	convert_(GetDlgItem(hDlg, convert)),
	progBar_(GetDlgItem(hDlg, progBar)),
	spectrLog_(GetDlgItem(hDlg, log)),

	calcSpectrWidth_(GetWindowWidth(calcSpectr_)),
	calcSpectrHeight_(GetWindowHeight(calcSpectr_)),
	spectrTitleWidth_(GetWindowWidth(spectrTitle_)),
	spectrTitleHeight_(GetWindowHeight(spectrTitle_)),
	convetWidth_(GetWindowWidth(convert_)),
	convertHeight_(GetWindowHeight(convert_)),
	progBarHeight_(GetWindowHeight(progBar_)),

	spectrWidth_(GetWindowWidth(spectr_)),
	spectrHeight_(GetWindowHeight(spectr_)),

	toRepaint_(true)
{}
PianoToMidi_Win::~PianoToMidi_Win() {}

void PianoToMidi_Win::FFmpegDecode(LPCSTR aFile)
{
	try
	{
		log_ += media_->FFmpegDecode(aFile);
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n");
		log_ += "\r\n";
		SetWindowTextA(spectrLog_, log_.c_str());
		Button_Enable(calcSpectr_, true);
	}
	catch (const FFmpegError& e)
	{
		SetWindowTextA(spectrLog_, e.what());
		MessageBoxA(hDlg_, e.what(), "Audio file error", MB_OK | MB_ICONHAND);
	}
}

void PianoToMidi_Win::Spectrum(const string& pathA)
{
	CursorWait cursor;
	Button_Enable(calcSpectr_, false);

	try
	{
		log_ += media_->CqtTotal() + "\r\n";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		InvalidateRect(hDlg_, nullptr, true);
	}
	catch (const CqtError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Spectrogram error", MB_OK | MB_ICONHAND);
		return;
	}

	try
	{
		log_ += media_->HarmPerc() + "\r\n";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
	}
	catch (const CqtError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Harmonic-Percussive separation error", MB_OK | MB_ICONHAND);
		return;
	}

	try
	{
		log_ += media_->Tempo() + "\r\n";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
	}
	catch (const CqtError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Tempogram error", MB_OK | MB_ICONHAND);
		return;
	}

	try
	{
		log_ += "\r\n" + media_->KerasLoad(pathA) + "\r\n";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // here it is required, because two lines
		SetWindowTextA(spectrLog_, log_.c_str());
	}
	catch (const KerasError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Neural network file error", MB_OK | MB_ICONHAND);
		return;
	}

	Button_Enable(convert_, true);
}

string PianoToMidi_Win::Convert(LPCTSTR mediaFile)
{
	CursorWait cursor;
	Button_Enable(convert_, false);

	try
	{
		// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
		// and code can loop indefinitely
#pragma warning(suppress:28159)
		const auto timeStart(GetTickCount());
		const auto percentStart(media_->CnnProbabs());
		bool alreadyAsked(false);
		for (auto percent(percentStart); percent < 100u; percent = media_->CnnProbabs())
		{
			SendMessage(progBar_, PBM_SETPOS, percent, 0);
			if (not alreadyAsked and percent >= percentStart + 1)
			{
				alreadyAsked = true;
				// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
				// and code can loop indefinitely
#pragma warning(suppress:28159)
				const auto seconds((GetTickCount() - timeStart) * (100 - percent) / 1'000);
				ostringstream os;
				os << "Conversion will take " << seconds / 60 << " min : "
					<< seconds % 60 << " sec" << endl << "Press OK if you are willing to wait.";
				if (MessageBoxA(hDlg_, os.str().c_str(), "Neural net in process...",
					MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON1) == IDCANCEL)
				{
					Button_Enable(convert_, true);
					return "";
				}
			}
		}
	}
	catch (const KerasError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Neural network forward pass error", MB_OK | MB_ICONHAND);
		return "";
	}
	SendMessage(progBar_, PBM_SETPOS, 99, 0);

	try { log_ += media_->Gamma() + "\r\n"; }
	catch (const KerasError& e)
	{
		log_ += string("\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Key signature error", MB_OK | MB_ICONHAND);
		return "";
	}
	InvalidateRect(hDlg_, nullptr, true);

	log_ += media_->KeySignature() + "\r\n";
	log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
	SetWindowTextA(spectrLog_, log_.c_str());

	OPENFILENAME fileName{ sizeof fileName, hDlg_ };
	fileName.lpstrFilter = TEXT("MIDI files (*.mid)\0")	TEXT("*.mid*\0");

	vector<TCHAR> fileT(static_cast<size_t>(GetFileTitle(mediaFile, nullptr, 0)));
	GetFileTitle(mediaFile, fileT.data(), static_cast<WORD>(fileT.size()));
	const auto extIter(find(fileT.crbegin(), fileT.crend(), '.'));
	if (extIter != fileT.crend()) fileT.resize(static_cast<size_t>(fileT.crend() - extIter - 1));

	TCHAR buf[MAX_PATH];
#pragma warning(suppress:4189) // Local variable is initialized but not referenced
	const auto unusedIter(copy(fileT.cbegin(), fileT.cend(), buf));
	buf[fileT.size()] = 0;
	fileName.lpstrFile = buf;
	fileName.nMaxFile = sizeof buf / sizeof *buf;

	fileName.Flags = OFN_OVERWRITEPROMPT;
	fileName.lpstrDefExt = TEXT("mid");

	while (not GetSaveFileName(&fileName)) if (MessageBox(hDlg_,
		TEXT("MIDI-file will NOT be saved! Are you sure?!\n")
		TEXT("You will have to convert the audio again from the beginning."),
		TEXT("File save error"), MB_ICONHAND | MB_YESNO | MB_DEFBUTTON2) == IDYES)
	{
		log_ += "\r\nMIDI-file not saved :(";
		SetWindowTextA(spectrLog_, log_.c_str());
		return "";
	}

#ifdef UNICODE
	const wstring fileW(fileName.lpstrFile);
	string fileA(fileW.cbegin(), fileW.cend());
#else
	string fileA(fileName.lpstrFile);
#endif
	try
	{
		media_->WriteMidi(fileA.c_str());
		log_ += "\r\n" + fileA + " saved.";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		SendMessage(progBar_, PBM_SETPOS, 100, 0);
	}
	catch (const MidiOutError& e)
	{
		log_ += string("\r\n") + e.what();
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "MIDI write error", MB_OK | MB_ICONHAND);
		return "";
	}

	return fileA;
}

void PianoToMidi_Win::OnPaint() const
{
	CanvasGdi canvas(hDlg_);
	if (media_->GetCqt().empty()) return;

	const auto cqt(media_->GetCqt());
	const auto cqtSize(min(static_cast<size_t>(spectrWidth_), cqt.size() / media_->GetNumBins()));
	wostringstream wos;
	if (cqtSize == static_cast<size_t>(spectrWidth_)) wos << TEXT("SPECTROGRAM OF THE FIRST ")
		<< media_->GetMidiSeconds() * cqtSize * media_->GetNumBins() / cqt.size() << TEXT(" SECONDS:");
	else wos << TEXT("SPECTROGRAM OF ALL THE ") << media_->GetMidiSeconds() << TEXT(" SECONDS:");
	Static_SetText(spectrTitle_, wos.str().c_str());

	if (not toRepaint_) return;

	const auto binWidth(spectrWidth_ / static_cast<LONG>(cqtSize));
	const auto binsPerPixel((media_->GetNumBins() - 1) / spectrHeight_ + 1);
	const auto cqtMax(*max_element(cqt.cbegin(), cqt.cbegin()
		+ static_cast<ptrdiff_t>(cqtSize * media_->GetNumBins())));

	const BitmapCompatible hBitmap(spectr_, spectrWidth_, spectrHeight_);

	const auto bottom((spectrHeight_ + (media_->GetNumBins() / binsPerPixel)) / 2);
	for (size_t i(0); i < cqtSize; ++i)
		for (size_t j(0); j < media_->GetNumBins() / binsPerPixel; ++j)
		{
			const auto bin(accumulate(cqt.cbegin() + static_cast<ptrdiff_t>(i * media_->GetNumBins()
				+ j * binsPerPixel), cqt.cbegin() + static_cast<ptrdiff_t>(i * media_->GetNumBins()
					+ (j + 1) * binsPerPixel), 0.f) * 5 / cqtMax / binsPerPixel);
			assert(bin >= 0 and bin <= 5 and "Wrong cqt-bin value");
			SetPixelV(hBitmap, static_cast<int>(i), static_cast<int>(bottom - j),
				bin < 1 ? RGB(0, 0, bin * 0xFF) : bin < 2 ? RGB(0, (bin - 1) * 0xFF, 0xFF) :
				bin < 3 ? RGB(0, 0xFF, 0xFF * (3 - bin)) : bin < 4
				? RGB((bin - 4) * 0xFF, 0xFF, 0) : RGB(0xFF, 0xFF * (5 - bin), 0));
		}

	if (media_->GetNotes().empty()) return;

	for (size_t i(0); i < media_->GetOnsetFrames().size(); ++i)
	{
		const auto onset(media_->GetOnsetFrames().at(i));
		if (onset > cqtSize) break;

		for (const auto& note : media_->GetNotes().at(i))
		{
			const auto bin(static_cast<int>(bottom
				- note.first * media_->GetNumBins() / 88 / binsPerPixel));
			SelectObject(hBitmap, GetStockPen(BLACK_PEN));
			SelectObject(hBitmap, GetStockBrush(WHITE_BRUSH));
			Ellipse(hBitmap, static_cast<int>(onset) - 5, bin - 5,
				static_cast<int>(onset) + 5, bin + 5);
		}
	}
}