#include "WinApp.h"


void WinApp::Initialze(HINSTANCE hInstance, HWND hwnd) {

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hInstance = GetModuleHandle(nullptr, IDC_ARROW;

	RegisterClass(&wc);

	const int32_t kClient = 1280;
	const int32_t kClientHeight = 720;

	RECT wrc = { 0,0,kClient,kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);


	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"CG2",
		WS_OVERLAPPEDWINDOW,
		CW_USEDDEFAULT,
		CW_USEDDEFAULT,
		Wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	ShowWindow(hwnd, SW_SHOW);
	
}

void WinApp::Update() {
}
