#include "Input.h"


void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {

	HRESULT hr;

	///DirectInput Setup
	hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	
	//Keyboard
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);

	// Mouse
	hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	hr = mouse->SetDataFormat(&c_dfDIMouse);
	hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

}

void Input::Update() {

	keyboard->Acquire();
	BYTE key[256] = {};
	keyboard->GetDeviceState(sizeof(key), key);

}
