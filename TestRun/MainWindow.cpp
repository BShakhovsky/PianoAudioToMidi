#include "stdafx.h"
#include "MainWindow.h"
#include "About.h"
#include "Spectrogram.h"
#include "Resource.h"

HINSTANCE MainWindow::hInstance = nullptr;
HWND MainWindow::hWndMain = nullptr;
TCHAR MainWindow::path[] = TEXT("");

BOOL MainWindow::OnCreate(const HWND, const LPCREATESTRUCT)
{
	GetCurrentDirectory(ARRAYSIZE(path), path);
	return true;
}

void MainWindow::OpenAudioFile(LPCTSTR fileName)
{
	Spectrogram::mediaFile = fileName;
	// Pointer to potentially throwing function passed to extern C function under -EHc.
	// Undefined behavior may occur if this function throws
#pragma warning(suppress:5039)
	DialogBox(MainWindow::hInstance, MAKEINTRESOURCE(IDD_SPECTRUM), MainWindow::hWndMain, Spectrogram::Main);
}
void MainWindow::OnDropFiles(const HWND, const HDROP hDrop)
{
	TCHAR fileName[MAX_PATH] = TEXT("");
	DragQueryFile(hDrop, 0, fileName, sizeof fileName / sizeof *fileName);
	OpenAudioFile(fileName);
	DragFinish(hDrop);
}

void MainWindow::OnCommand(const HWND hWnd, const int id, const HWND, const UINT)
{
	switch (id)
	{
	case ID_FILE_OPEN:
	{
		OPENFILENAME fileName{ sizeof fileName, hWnd };
		fileName.lpstrFilter = TEXT("All files\0") TEXT("*.*\0");
		TCHAR buf[MAX_PATH] = TEXT("");
		fileName.lpstrFile = buf;
		fileName.nMaxFile = sizeof buf / sizeof *buf;
		fileName.Flags = OFN_FILEMUSTEXIST;
		if (GetOpenFileName(&fileName)) OpenAudioFile(fileName.lpstrFile);
	}																					break;
	case IDM_ABOUT:
		// Pointer to potentially throwing function passed to extern C function under -EHc.
		// Undefined behavior may occur if this function throws
#pragma warning(suppress:5039)
					DialogBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);	break;
	case IDM_EXIT:	DestroyWindow(hWnd);												break;
	}
}

LRESULT CALLBACK MainWindow::WndProc(const HWND hWnd,
	const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	switch (msg)
	{
		HANDLE_MSG(hWnd, WM_CREATE,		OnCreate);
		HANDLE_MSG(hWnd, WM_DESTROY,	OnDestroy);
		HANDLE_MSG(hWnd, WM_DROPFILES,	OnDropFiles);
		HANDLE_MSG(hWnd, WM_COMMAND,	OnCommand);
	default: return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}