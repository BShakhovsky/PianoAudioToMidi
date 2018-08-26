#pragma once

class MainWindow abstract
{
public:
	static HINSTANCE hInstance;
	static HWND hWndMain;
	static TCHAR path[MAX_PATH];

	static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
private:
#pragma warning(push)
#pragma warning(disable:4514) // Unreferenced inline function has been removed
	static BOOL OnCreate(HWND, LPCREATESTRUCT)
	{
		GetCurrentDirectory(ARRAYSIZE(path), path);
		return true;
	}
	static void OnDestroy(HWND) { PostQuitMessage(0); }

	static void OnDropFiles(HWND, const HDROP hDrop)
	{
		TCHAR fileName[MAX_PATH] = TEXT("");
		DragQueryFile(hDrop, 0, fileName, sizeof fileName / sizeof *fileName);
		OpenAudioFile(fileName);
		DragFinish(hDrop);
	}
#pragma warning(pop)
	static void OpenAudioFile(LPCTSTR);

	static void OnCommand(HWND, int, HWND, UINT);
};