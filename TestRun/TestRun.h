#pragma once

class TestRun abstract
{
public:
	static constexpr auto windowTitle = TEXT("Piano Audio To MIDI");

	static int Main(int);
private:
	static ATOM MyRegisterClass();
	static BOOL InitInstance(int);

	static constexpr LPCTSTR szWindowClass = TEXT("MainWindow");
};