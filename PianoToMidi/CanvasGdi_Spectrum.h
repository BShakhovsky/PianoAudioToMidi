#pragma once

class CanvasGdi_Spectrum
{
public:
	explicit CanvasGdi_Spectrum(HWND hWnd) : hWnd_(hWnd), hDC_(BeginPaint(hWnd_, &ps_)) {}
	~CanvasGdi_Spectrum() { EndPaint(hWnd_, &ps_); }
#pragma warning(suppress:4514) // Unreferenced inline function has been removed
	operator HDC () const { return hDC_; }
private:
	const HWND hWnd_;
	PAINTSTRUCT ps_;
	const HDC hDC_;

	const CanvasGdi_Spectrum& operator=(const CanvasGdi_Spectrum&) = delete;
};