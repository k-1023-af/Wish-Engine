#include <Windows.h>
#include <cstdint>

const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	WNDCLASS wc{};

	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);

	OutputDebugStringA("Hello,DirectX!\n");

	RECT wrc = { 0,0,kClientWidth,kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
		wc.lpszClassName,		//利用するクラス名
		L"CG2",					//タイトルバーの文字　（何でもよい)
		WS_OVERLAPPEDWINDOW,	//よく見るウインドウスタイル
		CW_USEDEFAULT,			//表示X座標　（Windowsに任せる）
		CW_USEDEFAULT,			//表示Y座標　（Windowsに任せる）
		wrc.right - wrc.left,	//ウインドウ横幅
		wrc.bottom - wrc.top,	//ウインドウ縦幅
		nullptr,				//親ウインドウハンドル
		nullptr,				//メニューハンドル
		wc.hInstance,			//インスタンスハンドル
		nullptr);				//オプション

	//ウインドウの表示する
	ShowWindow(hwnd, SW_SHOW);

	MSG msg{};

	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			//ゲームの処理
		}
	}

	return 0;
}