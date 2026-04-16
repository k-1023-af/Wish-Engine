#include <Windows.h>
#include <cstdint>

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int){
	
	WNDCLASS wc{};

	wc.lpfnWndProc = WndProc;

	wc.lpszClassName = L"CG2WindowClass";

	wc.hInstance = GetModuleHandle(nullptr);

	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);

	OutputDebugStringA("Hello,DirectX!\n");
	
	return 0;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {

	switch (message) {

	case WM_DESTROY:

		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, message, wparam, lparam);

}

