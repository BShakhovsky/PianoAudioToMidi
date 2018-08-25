#pragma once

class BitmapCompatible
{
public:
	explicit BitmapCompatible(HWND, int cx, int cy);
	~BitmapCompatible();
	operator const HDC() const;
private:
	const std::unique_ptr<class CanvasGdi> canvas_;
	const std::unique_ptr<class DeviceCompatible> hDCmem_;
	const HBITMAP hBitmap_;
	const int cx_, cy_;

	BitmapCompatible(const BitmapCompatible&) = delete;
	const BitmapCompatible& operator=(const BitmapCompatible&) = delete;
};