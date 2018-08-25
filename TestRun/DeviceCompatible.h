#pragma once

class DeviceCompatible
{
public:
	explicit DeviceCompatible(const HDC hDC) : hDC_(CreateCompatibleDC(hDC)) {}
	~DeviceCompatible() { DeleteDC(hDC_); }
	operator const HDC() const { return hDC_; }
private:
	const HDC hDC_;

	const DeviceCompatible& operator=(const DeviceCompatible&) = delete;
};