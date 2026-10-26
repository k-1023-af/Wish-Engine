#pragma once
#include "Windows.h"
#include <cstdint>

class WinApp {
	//namespace
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	void Initialze(HINSTANCE hInstance, HWND hwnd);
	void Update();

};

