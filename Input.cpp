#include "Input.h"
#include <cassert>

void Input::Initialize() {

	///DirectInput Setup
	Microsoft::WRL::ComPtr<IDirectInput8> directInput = nullptr;
	hr = DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(hr));

	//Keyboard
	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard = nullptr;
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(hr));
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));
	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	//// Mouse
	//Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse = nullptr;
	//hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	//assert(SUCCEEDED(hr));
	//hr = mouse->SetDataFormat(&c_dfDIMouse);
	//assert(SUCCEEDED(hr));
	//hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	//assert(SUCCEEDED(hr));


}

void Input::Update()
{
}
