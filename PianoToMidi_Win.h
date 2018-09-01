#pragma once
#pragma comment(lib, "PianoToMidi")

class PianoToMidi_Win
{
	static constexpr LONG edge_ = 10;
public:
	explicit PianoToMidi_Win(HWND dialog, int calcSpectrButton, int spectrTitle,
		int convertButton, int progBar, int spectrLog, int spectrPictureBox);
	~PianoToMidi_Win();

	void FFmpegDecode(LPCSTR fileNameA);
	void Spectrum(const std::string& currExePathA);
	std::string Convert(LPCTSTR fullMediaFileName);
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	void FFmpegDecode(LPCWSTR fileNameW)
	{
		const std::wstring wFile(fileNameW);
		const std::string aFile(wFile.cbegin(), wFile.cend());
		FFmpegDecode(aFile.c_str());
	}
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
	void OnSize(int cx, int cy)
	{
		SetWindowPos(calcSpectr_, nullptr,
			(cx - calcSpectrWidth_) / 2, edge_,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);

		SetWindowPos(spectrTitle_, nullptr,
			(cx - spectrTitleWidth_) / 2, calcSpectrHeight_ + 2 * edge_,
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
#pragma warning(pop)
#pragma warning(suppress:4711) // Automatic inline expansion
	void OnPaint() const;
private:
	const std::unique_ptr<class PianoToMidi> media_;

	const HWND hDlg_, calcSpectr_, spectrTitle_, spectr_, convert_, progBar_, spectrLog_;
	const LONG calcSpectrWidth_, calcSpectrHeight_,
		spectrTitleWidth_, spectrTitleHeight_,
		convetWidth_, convertHeight_, progBarHeight_;
	LONG spectrWidth_, spectrHeight_;

	std::string log_;
	bool toRepaint_;
	const byte padding_[sizeof(intptr_t) - sizeof(bool)] = { 0 };

	PianoToMidi_Win(const PianoToMidi_Win&) = delete;
	const PianoToMidi_Win& operator=(const PianoToMidi_Win&) = delete;
};