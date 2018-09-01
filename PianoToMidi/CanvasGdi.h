#pragma once

class CanvasGdi
{
public:
	explicit CanvasGdi(HWND hWnd) : hWnd_(hWnd), hDC_(BeginPaint(hWnd_, &ps_)) {}
	~CanvasGdi() { EndPaint(hWnd_, &ps_); }
#pragma warning(suppress:4514) // Unreferenced inline function has been removed
	operator HDC () const { return hDC_; }
private:
	const HWND hWnd_;
	PAINTSTRUCT ps_;
	const HDC hDC_;

	const CanvasGdi& operator=(const CanvasGdi&) = delete;
};