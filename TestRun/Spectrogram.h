#pragma once

class Spectrogram abstract
{
	static HWND calcSpectr, spectrTitle, spectr, convert, progBar, spectrLog;
	static LONG calcSpectrWidth, calcSpectrHeight, spectrTitleWidth, spectrTitleHeight,
		spectrWidth, spectrHeight, convetWidth, convertHeight, progBarHeight;
	static constexpr LONG edge = 10;
public:
	static LPCTSTR mediaFile;

	static INT_PTR CALLBACK Main(HWND, UINT, WPARAM, LPARAM);
private:
	static BOOL OnInitDialog(HWND, HWND, LPARAM);
	static void OnDestroyDialog(HWND);
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	static void OnSize(HWND hDlg, UINT, int cx, int cy)
	{
		SetWindowPos(calcSpectr, nullptr,
			(cx - calcSpectrWidth) / 2, edge,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);

		SetWindowPos(spectrTitle, nullptr,
			(cx - spectrTitleWidth) / 2, calcSpectrHeight + 2 * edge,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);

		spectrWidth = cx - 2 * edge;
		spectrHeight = cy / 2 - calcSpectrHeight - spectrTitleHeight - 3 * edge;
		SetWindowPos(spectr, nullptr,
			edge, calcSpectrHeight + spectrTitleHeight + 3 * edge,
			spectrWidth, spectrHeight, SWP_NOZORDER);

		SetWindowPos(convert, nullptr,
			(cx - convetWidth) / 2, cy / 2 + edge,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);

		SetWindowPos(progBar, nullptr, edge,
			cy / 2 + convertHeight + 2 * edge,
			cx - 2 * edge, progBarHeight, SWP_NOZORDER);

		SetWindowPos(spectrLog, nullptr, edge,
			cy / 2 + convertHeight + progBarHeight + 3 * edge, cx - 2 * edge,
			cy / 2 - convertHeight - progBarHeight - 4 * edge, SWP_NOZORDER);

		InvalidateRect(hDlg, nullptr, true);
	}
#pragma warning(pop)
	static void OnPaint(HWND);
	static void OnCommand(HWND, int, HWND, UINT);

	static std::unique_ptr<class PianoToMidi> media;
	static std::string log;
	static bool toRepaint, midiWritten;
};