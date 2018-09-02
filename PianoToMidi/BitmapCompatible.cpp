#include "stdafx.h"
#include "BitmapCompatible.h"
#include "CanvasGdi_Spectrum.h"
#include "DeviceCompatible.h"

using std::make_unique;

BitmapCompatible::BitmapCompatible(const HWND hWnd, const int cx, const int cy)
	: canvas_(make_unique<CanvasGdi_Spectrum>(hWnd)),
	hDCmem_(make_unique<DeviceCompatible>(*canvas_)),
	hBitmap_(CreateCompatibleBitmap(*canvas_, cx, cy)),
	cx_(cx), cy_(cy)
{
	SelectBitmap(*hDCmem_, hBitmap_);
}

BitmapCompatible::~BitmapCompatible()
{
	BitBlt(*canvas_, 0, 0, cx_, cy_, *hDCmem_, 0, 0, SRCCOPY);
	DeleteBitmap(hBitmap_);
}

BitmapCompatible::operator const HDC() const { return *hDCmem_; }