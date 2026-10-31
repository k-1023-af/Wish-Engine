#include "Input.h"


void Input::Initialize(WinApp* winApp) {
	this->winApp_ = winApp;

	HRESULT hr;

	///DirectInput Setup
	hr = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	
	//Keyboard
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// Mouse
	hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	hr = mouse->SetDataFormat(&c_dfDIMouse);
	hr = mouse->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(hr));

}

void Input::Update() {
	memcpy(keyPre, key, sizeof(key));

	keyboard->Acquire();
	keyboard->GetDeviceState(sizeof(key), key);

	//if (key[DIK_0]) {
	//	OutputDebugStringA("Hit 0\n");
	//}

	//mouse->Acquire();

}

bool Input::PushKey(BYTE keyNumber) { 
	if (key[keyNumber]) { return true; }
	return false; 
}

bool Input::TriggerKey(BYTE keyNumber) {
	if (key[keyNumber] && !keyPre[keyNumber]) { return true; }
	return false;
}
