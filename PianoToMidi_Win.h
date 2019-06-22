#pragma once
#pragma comment(lib, "PianoToMidi")

class PianoToMidi_Win
{
	static constexpr LONG edge_ = 10;
public:
	explicit PianoToMidi_Win(HWND dialog, int calcSpectrButton, int spectrTitle,
		int radioGroup, int radioCqt, int radioMel,
		int convertButton, int progBar, int spectrLog, int spectrPictureBox);
	~PianoToMidi_Win();

	void FFmpegDecode(LPCWSTR fileNameW);
	void FFmpegDecode(LPCSTR fileNameA);
	void Spectrum(const std::string& currExePathA);
	std::string Convert(LPCTSTR fullMediaFileName);
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	void Spectrum(const std::wstring& currExePathW)
	{
		Spectrum(std::string(currExePathW.cbegin(), currExePathW.cend()));
	}

	void OnEnterSizeMove() { toRepaint_ = false; }
	void OnExitSizeMove()
	{
		toRepaint_ = true;
		InvalidateRect(hDlg_, nullptr, true);
	}
#pragma warning(pop)
	void OnSize(int cx, int cy);
	void SpecType(bool isCqt);
	void OnPaint() const;
private:
	const std::unique_ptr<class PianoToMidi> media_;

	const HWND hDlg_, calcSpectr_, spectrTitle_,
		radioGroup_, radioCqt_, radioMel_,
		spectr_, convert_, progBar_, spectrLog_;
	const LONG calcSpectrWidth_, calcSpectrHeight_,
		spectrTitleWidth_, spectrTitleHeight_,
		convetWidth_, convertHeight_, progBarHeight_;
	LONG spectrWidth_, spectrHeight_,
		radioGroupWidth_, radioGroupHeight_;
#ifdef _WIN64
	const byte pad_[4]{ 0 };
#endif

	std::string log_;
	bool toRepaint_, isCqt_;
	const byte padding_[sizeof(intptr_t) - sizeof toRepaint_ - sizeof isCqt_] = { 0 };

	PianoToMidi_Win(const PianoToMidi_Win&) = delete;
	const PianoToMidi_Win& operator=(const PianoToMidi_Win&) = delete;
};