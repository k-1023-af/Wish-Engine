#pragma once
#include "Windows.h"

class WinApp {
	//namespace
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	void Initialze(HINSTANCE hInstance, HWND hwnd);
	void Update();

};

