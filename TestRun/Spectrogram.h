#pragma once

class Spectrogram abstract
{
	static HWND spectrTitle, spectr, spectrLog, progBar, calcSpectr, convert;
	static LONG spectrTitleWidth, spectrTitleHeight,
		spectrWidth, spectrHeight, progBarHeight,
		calcSpectrWidth, calcSpectrHeight, convetWidth, convertHeight, edge;
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
	static bool midiWritten;
};