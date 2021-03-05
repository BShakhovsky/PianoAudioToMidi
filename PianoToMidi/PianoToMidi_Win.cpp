#include "stdafx.h"
#include "PianoToMidi_Win.h"
#include "PianoToMidi.h"
#include "CursorWait.h"
#include "CanvasGdi_Spectrum.h"
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
	const int radioGroup, const int radioCqt, const int radioMel,
	const int convert, const int progBar, const int log, const int spectr)
	: media_(make_unique<PianoToMidi>()),
	hDlg_(hDlg),
	calcSpectr_(GetDlgItem(hDlg, calcSpectr)),
	spectrTitle_(GetDlgItem(hDlg, spectrTitle)),

	radioGroup_(GetDlgItem(hDlg, radioGroup)),
	radioCqt_(GetDlgItem(hDlg, radioCqt)),
	radioMel_(GetDlgItem(hDlg, radioMel)),

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
	radioGroupWidth_(GetWindowWidth(radioGroup_)),
	radioGroupHeight_(GetWindowHeight(radioGroup_)),

	toRepaint_(true), isCqt_(true)
{}
PianoToMidi_Win::~PianoToMidi_Win() {}

string Utf8Decode(LPCWSTR strW) {
	string strA(WideCharToMultiByte(CP_UTF8, 0, strW, -1, nullptr, 0, nullptr, nullptr), '\0');
	WideCharToMultiByte(CP_UTF8, 0, strW, -1, strA.data(), strA.size(), nullptr, nullptr);
	return move(strA);
}

void PianoToMidi_Win::FFmpegDecode(LPCWSTR fileNameW)
{
	FFmpegDecode(Utf8Decode(fileNameW).c_str());
}
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
	Button_SetCheck(isCqt_ ? radioCqt_ : radioMel_, true);

	CursorWait cursor;
	Button_Enable(calcSpectr_, false);

	try
	{
		log_ += media_->MelSpectrum() + "\r\n";
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
	}
	catch (const MelError& e)
	{
		log_ += string("\r\n") + e.what();
		log_ = regex_replace(log_, regex("\r?\n\r?"), "\r\n"); // just in case
		SetWindowTextA(spectrLog_, log_.c_str());
		MessageBoxA(hDlg_, e.what(), "Mel Transform error", MB_OK | MB_ICONHAND);
		return;
	}

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
	DisableProcessWindowsGhosting();

	CursorWait cursor;
	Button_Enable(convert_, false);

	try
	{
		// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
		// and code can loop indefinitely
#pragma warning(suppress:28159)
		const auto timeStart(GetTickCount());
		bool alreadyAsked(false);
		for (auto percent(media_->RnnProbabs()); percent < 100u; percent = media_->RnnProbabs())
		{
			SendMessage(progBar_, PBM_SETPOS, percent, 0);
			if (not alreadyAsked and percent >= 1)
			{
				alreadyAsked = true;
				// Consider using 'GetTickCount64' : GetTickCount overflows every 49 days,
				// and code can loop indefinitely
#pragma warning(suppress:28159)
				const auto seconds((GetTickCount() - timeStart) / percent / 10);
				ostringstream os;
				os << "Conversion will take " << seconds / 60 << " min : "
					<< seconds % 60 << " sec" << endl << "Press OK if you are willing to wait.";

				SendMessage(progBar_, PBM_SETSTATE, PBST_ERROR, 0);
				SendMessage(progBar_, PBM_SETBARCOLOR, 0, RGB(0xFF, 0, 0));
				if (MessageBoxA(hDlg_, os.str().c_str(), "Neural net in process...",
					MB_ICONQUESTION | MB_OKCANCEL | MB_DEFBUTTON1) == IDCANCEL)
				{
					Button_Enable(convert_, true);
					return "";
				}
				SendMessage(progBar_, PBM_SETBARCOLOR, 0, CLR_DEFAULT);
				SendMessage(progBar_, PBM_SETSTATE, PBST_NORMAL, 0);
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
	const auto fileA(Utf8Decode(fileName.lpstrFile));
#else
	string fileA(fileName.lpstrFile);
#endif
	try
	{
		media_->WriteMidi(fileName.lpstrFile, fileA);
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

void PianoToMidi_Win::OnSize(const int cx, const int cy)
{
	SetWindowPos(calcSpectr_, nullptr,
		(cx - calcSpectrWidth_) / 2, edge_,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);

	SetWindowPos(spectrTitle_, nullptr,
		(cx - spectrTitleWidth_) / 2, calcSpectrHeight_ + 2 * edge_,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);


	RECT rect, newRect;
	GetWindowRect(radioGroup_, &rect);
	SetWindowPos(radioGroup_, nullptr,
		(cx - spectrTitleWidth_) / 2 - radioGroupWidth_ - edge_, calcSpectrHeight_ + spectrTitleHeight_ - radioGroupHeight_ + 3 * edge_,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);
	GetWindowRect(radioGroup_, &newRect);
	const auto deltaX(newRect.left - rect.left), deltaY(newRect.top - rect.top);

	GetWindowRect(radioCqt_, &rect);
	ScreenToClient(hDlg_, reinterpret_cast<LPPOINT>(&rect));
	SetWindowPos(radioCqt_, nullptr,
		rect.left + deltaX, rect.top + deltaY,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);

	GetWindowRect(radioMel_, &rect);
	ScreenToClient(hDlg_, reinterpret_cast<LPPOINT>(&rect));
	SetWindowPos(radioMel_, nullptr,
		rect.left + deltaX, rect.top + deltaY,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);


	spectrWidth_ = cx - 2 * edge_;
	spectrHeight_ = cy / 2 - calcSpectrHeight_ - spectrTitleHeight_ - 3 * edge_;
	SetWindowPos(spectr_, nullptr,
		edge_, calcSpectrHeight_ + spectrTitleHeight_ + 3 * edge_,
		spectrWidth_, spectrHeight_, SWP_NOZORDER);

	SetWindowPos(convert_, nullptr,
		(cx - convetWidth_) / 2, cy / 2 + edge_,
		0, 0, SWP_NOSIZE | SWP_NOZORDER);

	SetWindowPos(progBar_, nullptr, edge_,
		cy / 2 + convertHeight_ + 2 * edge_,
		cx - 2 * edge_, progBarHeight_, SWP_NOZORDER);

	SetWindowPos(spectrLog_, nullptr, edge_,
		cy / 2 + convertHeight_ + progBarHeight_ + 3 * edge_, cx - 2 * edge_,
		cy / 2 - convertHeight_ - progBarHeight_ - 4 * edge_, SWP_NOZORDER);

	InvalidateRect(hDlg_, nullptr, true);
}

void PianoToMidi_Win::SpecType(const bool isCqt)
{
	isCqt_ = isCqt;
	OnPaint();
	InvalidateRect(hDlg_, nullptr, true);
}

void PianoToMidi_Win::OnPaint() const
{
	using namespace Gdiplus;

	CanvasGdi_Spectrum canvasDlg(hDlg_);

	if (isCqt_ ? media_->GetCqt().empty() : media_->GetMel().empty()) return;

	const auto spec(isCqt_ ? media_->GetCqt() : media_->GetMel());
	const auto nBins(isCqt_ ? media_->GetNumBins() : media_->nMels),
		specSize(min(static_cast<size_t>(spectrWidth_), spec.size() / nBins));
	wostringstream wos;
	if (specSize == static_cast<size_t>(spectrWidth_)) wos << TEXT("SPECTROGRAM OF THE FIRST ")
		<< media_->GetMidiSeconds() * specSize * nBins / spec.size() << TEXT(" SECONDS:");
	else wos << TEXT("SPECTROGRAM OF ALL THE ") << media_->GetMidiSeconds() << TEXT(" SECONDS:");
	Static_SetText(spectrTitle_, wos.str().c_str());

	if (not toRepaint_) return;

	const BitmapCompatible hBitmap(spectr_, spectrWidth_, spectrHeight_);

	ULONG_PTR token;
	GdiplusStartupInput gdiInput;
	GdiplusStartup(&token, &gdiInput, nullptr);

	Graphics gf(hBitmap);
	const auto binsPerPixel((nBins - 1) / spectrHeight_ + 1);
	const auto pixelsPerBin(max(1.f, Divide<REAL>(spectrHeight_ - 1, nBins)));
	gf.TranslateTransform(0, (static_cast<float>(spectrHeight_) + Multiply(static_cast<unsigned long>(nBins) / binsPerPixel, pixelsPerBin)) / 2);
	gf.ScaleTransform(1, pixelsPerBin);

	const auto specMinMax(minmax_element(spec.cbegin(), spec.cbegin() + static_cast<ptrdiff_t>(specSize * nBins)));
	for (size_t i(0); i < specSize; ++i) for (size_t j(0); j < nBins / binsPerPixel; ++j)
	{
		const auto bin((Divide(accumulate(spec.cbegin() + static_cast<ptrdiff_t>(i * nBins
			+ j * binsPerPixel), spec.cbegin() + static_cast<ptrdiff_t>(i * nBins
				+ (j + 1) * binsPerPixel), 0.f), binsPerPixel) - *specMinMax.first) / (*specMinMax.second - *specMinMax.first) * 5);
		assert(bin >= 0 and bin <= 5 and "Wrong cqt-bin value");
		const SolidBrush brush(bin < 1 ? Gdiplus::Color(0, 0, static_cast<BYTE>(bin * 0xFF))
			: bin < 2 ? Gdiplus::Color(0, static_cast<BYTE>((bin - 1) * 0xFF), 0xFF)
			: bin < 3 ? Gdiplus::Color(0, 0xFF, static_cast<BYTE>(0xFF * (3 - bin)))
			: bin < 4 ? Gdiplus::Color(static_cast<BYTE>((bin - 4) * 0xFF), 0xFF, 0)
			: Gdiplus::Color(0xFF, static_cast<BYTE>(0xFF * (5 - bin)), 0));
		gf.FillRectangle(&brush, static_cast<int>(i), -static_cast<int>(j), 1, 1);
	}

	const auto logScale(Divide(isCqt_ ? Divide(nBins, 88) : 1.f, binsPerPixel));
	if (not isCqt_)
	{
		const Pen pen(static_cast<ARGB>(Gdiplus::Color::Red), 1);
		for (const auto& octave : media_->GetMelOctaves())
		{
			const auto bin(-Multiply(logScale, octave));
			gf.DrawLine(&pen, 0.f, bin + 1, static_cast<REAL>(specSize), bin);
		}
	}

	if (media_->GetOnsets().empty()) return;

	const Pen pen(static_cast<ARGB>(Gdiplus::Color::Black), 1);
	const SolidBrush brush(static_cast<ARGB>(Gdiplus::Color::White));
	const auto DrawNotes([this, specSize, pixelsPerBin, logScale, &gf, &pen, &brush](const fdeep::float_vec& notes, const bool toFill, const float size)
		{
			for (size_t i(0); i < min(specSize, notes.size() / 88); ++i) for (size_t j(0); j < 88; ++j)
			{
				if (notes.at(i * 88 + j) < .5) continue;

				const auto bin(-Multiply(logScale, isCqt_ ? j : media_->GetMelNoteIndices().at(j)));
				if (toFill) gf.FillEllipse(&brush, static_cast<REAL>(i), bin - size / pixelsPerBin / 2 + 1, size, size / pixelsPerBin);
				gf.DrawEllipse(&pen, static_cast<REAL>(i), bin - size / pixelsPerBin / 2 + 1, size, size / pixelsPerBin);
			}
		});
	DrawNotes(media_->GetActives(), false, 4);
	DrawNotes(media_->GetOnsets(), true, 10);
}