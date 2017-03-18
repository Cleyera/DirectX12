#include <Windows.h>
#include <tchar.h>
#include "D3D12Manager.h"

namespace{
constexpr int WINDOW_WIDTH  = 640;
constexpr int WINDOW_HEIGHT = 480;
constexpr LPSTR WND_CLASS_NAME = _T("DirectX12");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd){
	WNDCLASSEX	wc{};
	HWND hwnd{};

	wc.cbSize			= sizeof(WNDCLASSEX);
	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= WindowProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= hInst;
	wc.lpszMenuName		= NULL;
	wc.lpszClassName	= WND_CLASS_NAME;
	wc.hIcon			= (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hIconSm			= (HICON)LoadImage(NULL, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_SHARED);
	wc.hbrBackground	= (HBRUSH)GetStockObject(BLACK_BRUSH);

	if(RegisterClassEx(&wc) == 0){ 
		return -1;
	}

	hwnd = CreateWindowEx(
		WS_EX_COMPOSITED,
		WND_CLASS_NAME,
		WND_CLASS_NAME,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		NULL,
		NULL,
		hInst,
		NULL);

	{
		int screen_width  = GetSystemMetrics(SM_CXSCREEN);
		int screen_height = GetSystemMetrics(SM_CYSCREEN);
		RECT rect{};
		GetClientRect(hwnd, &rect);
		MoveWindow(
			hwnd,
			(screen_width / 2) - ((WINDOW_WIDTH + (WINDOW_WIDTH - rect.right)) / 2), //ウインドウを画面
			(screen_height / 2) - ((WINDOW_HEIGHT + (WINDOW_HEIGHT - rect.bottom)) / 2),//中央に持ってくる
			WINDOW_WIDTH + (WINDOW_WIDTH - rect.right),
			WINDOW_HEIGHT + (WINDOW_HEIGHT - rect.bottom),
			TRUE);
	}

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	D3D12Manager direct_3d(hwnd, WINDOW_WIDTH, WINDOW_HEIGHT);

	while(TRUE){
		MSG msg{};
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)){
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if(msg.message == WM_QUIT){break;}
		}

		direct_3d.Render();

	}

	UnregisterClass(WND_CLASS_NAME, NULL); hwnd = NULL;

	return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
	switch (uMsg) {
		case WM_DESTROY:PostQuitMessage(0);	return -1;
		default:		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
