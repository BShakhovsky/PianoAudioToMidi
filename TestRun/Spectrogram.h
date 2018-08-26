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
	static void OnSize(HWND, UINT, int, int);
	static void OnPaint(HWND);
	static void OnCommand(HWND, int, HWND, UINT);

	static std::unique_ptr<class PianoToMidi> media;
	static std::string log;
	static bool toRepaint, midiWritten;
};