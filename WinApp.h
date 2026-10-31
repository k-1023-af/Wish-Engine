#pragma once
#include "Windows.h"
#include <cstdint>
#include <stdint.h>
//#include <wrl.h>
#include "externals/imgui/imgui.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static const int32_t kClientWidth = 1280;
static const int32_t kClientHeight = 720;

class WinApp {
	//namespace
	//template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	void Initialize();
	void Update();
	void Finalize();

	HWND GetHwnd() const { return hwnd; }
	HINSTANCE GetHInstance() const { return wc.hInstance; }

	bool ProcessMessage();

private:
	HWND hwnd = nullptr;
	WNDCLASS wc{};

};

