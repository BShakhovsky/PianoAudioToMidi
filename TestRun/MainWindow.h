#pragma once

class MainWindow abstract
{
public:
	static HINSTANCE hInstance;
	static HWND hWndMain;
	static TCHAR path[MAX_PATH];

	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
private:
	static BOOL OnCreate(HWND, LPCREATESTRUCT);
#pragma warning(suppress:4514) // Unreferenced inline function has been removed
	static void OnDestroy(HWND) { PostQuitMessage(0); }

	static void OpenAudioFile(LPCTSTR);
	static void OnDropFiles(HWND, HDROP);
	static void OnCommand(HWND, int, HWND, UINT);
};