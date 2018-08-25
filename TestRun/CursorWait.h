#pragma once

class CursorWait
{
public:
	CursorWait() : hCursorOld_(SetCursor(LoadCursor(nullptr, IDC_WAIT))) {}
	~CursorWait() { SetCursor(hCursorOld_); }
private:
	const HCURSOR hCursorOld_;

	const CursorWait& operator=(const CursorWait&) = delete;
};