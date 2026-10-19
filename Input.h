#pragma once
#include <cassert>
#include <windows.h>
//#define DIRECTINPUT_VERSION	0x800
#include <dinput.h>
#include <filesystem>
#include <wrl.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input
{
public:
	//namespace
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	void Update();

	bool PushKey(BYTE keyNumber);
	bool TriggerKey(BYTE keynumber);

private:
	BYTE key[256] = {};
	BYTE keyPre[256] = {};

	ComPtr<IDirectInput8> directInput = nullptr;
	ComPtr<IDirectInputDevice8> keyboard = nullptr;
	ComPtr<IDirectInputDevice8> mouse = nullptr;


	//BYTE gKeys[256] = {};
	//DIMOUSESTATE gMouseState = {};
	//
	//LPDIRECTINPUT8 gDirectInput = nullptr;
	//LPDIRECTINPUTDEVICE8 gKeyboard = nullptr;
	//LPDIRECTINPUTDEVICE8 gMouse = nullptr;

};





//
//void InitializeInput(HWND hwnd) {
//	DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&gDirectInput, nullptr);
//
//	// Keyboard
//	gDirectInput->CreateDevice(GUID_SysKeyboard, &gKeyboard, nullptr);
//	gKeyboard->SetDataFormat(&c_dfDIKeyboard);
//	gKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
//
//	// Mouse
//	gDirectInput->CreateDevice(GUID_SysMouse, &gMouse, nullptr);
//	gMouse->SetDataFormat(&c_dfDIMouse);
//	gMouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_EXCLUSIVE);
//}



//----////////DirectX Initialize process
	//----
	//----///DirectInput Setup
	//----IDirectInput8* directInput = nullptr;
	//----hr = DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	//----assert(SUCCEEDED(hr));
	//----
	//----//Keyboard
	//----IDirectInputDevice8* keyboard = nullptr;
	//----hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	//----assert(SUCCEEDED(hr));
	//----hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	//----assert(SUCCEEDED(hr));
	//----hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	//----assert(SUCCEEDED(hr));
	//----
	//----// Mouse
	//----IDirectInputDevice8* mouse = nullptr;
	//----hr = directInput->CreateDevice(GUID_SysMouse, &mouse, nullptr);
	//----assert(SUCCEEDED(hr));
	//----hr = mouse->SetDataFormat(&c_dfDIMouse);
	//----assert(SUCCEEDED(hr));
	//----hr = mouse->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	//----assert(SUCCEEDED(hr));
	//----
	//----ShowCursor(TRUE);
	//----ClipCursor(NULL);




//-----// ====== Sound and Camera Input ======== //
			//-----static BYTE prevKeys[256] = {};
			//-----keyboard->Acquire();
			//-----keyboard->GetDeviceState(sizeof(gKeys), gKeys);
			//-----
			//-----// Toggle between Debug and Regular camera (Press SPACE)
			//-----bool spaceKeyPressed = (gKeys[DIK_SPACE] & 0x80) && !(prevKeys[DIK_SPACE] & 0x80);
			//-----memcpy(prevKeys, gKeys, sizeof(prevKeys));
			//-----
			//-----if (spaceKeyPressed) {  // Key just pressed
			//-----	//useDebugCamera = !useDebugCamera;
			//-----	isSoundPlaying = !isSoundPlaying;
			//-----	SoundPlayWave(xAudio2.Get(), soundData1, isSoundPlaying);
			//-----}
			//-----
			//-----mouse->Acquire();
			//-----HRESULT hr = mouse->GetDeviceState(sizeof(DIMOUSESTATE), &gMouseState);


			//-----float mouseDeltaX = 0;
			//-----float mouseDeltaY = 0;
			//-----float mouseDeltaZ = (float)gMouseState.lZ; //wheel
			//-----
			//-----bool isLeftClick = false;
			//-----bool isRightClick = false;
			//-----
			//-----if (SUCCEEDED(hr)) {
			//-----	mouseDeltaX = (float)gMouseState.lX;
			//-----	mouseDeltaY = (float)gMouseState.lY;
			//-----	isLeftClick = (gMouseState.rgbButtons[0] & 0x80) != 0;
			//-----	isRightClick = (gMouseState.rgbButtons[1] & 0x80) != 0;
			//-----}
			//-----
			//-----if (useDebugCamera) {
			//-----	if (isLeftClick) {
			//-----		debugCamera.SetMouseDelta(mouseDeltaX, mouseDeltaY);      // Yaw + Pitch
			//-----	}
			//-----	if (isRightClick) {
			//-----		debugCamera.SetMouseRollDelta(mouseDeltaX);          // Roll with horizontal drag
			//-----	}
			//-----	if (mouseDeltaZ != 0) {
			//-----		debugCamera.SetMouseWheelDelta(mouseDeltaZ);      // Optional: forward movement
			//-----	}
			//-----	debugCamera.Update();
			//-----}



//if (Input::GetInstance()->TriggerKey(DIK_SPACE))
			//SoundPlayWave(xAudio2.Get(), soundData1);


			//BYTE key[256] = {};
			//if (key[DIK_0]) {
			//	OutputDebugStringA("Hit 0\n");
			//}

			//key input (Trigger) process
			//bool is the key being pressed? (uint8_t [key #]
			//bool the moment a key is pressed (uint8_t [key #]
			//prevents negative values and limitted to [0 - 255]