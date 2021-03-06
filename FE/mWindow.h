#pragma once

typedef BOOL
(WINAPI* GetWindowRect_T)(
	_In_ HWND hWnd,
	_Out_ LPRECT lpRect);

typedef BOOL
(WINAPI* GetClientRect_T)(
	_In_ HWND hWnd,
	_Out_ LPRECT lpRect);

typedef BOOL
(WINAPI *SetWindowPos_T)(
    _In_ HWND hWnd,
    _In_opt_ HWND hWndInsertAfter,
    _In_ int X,
    _In_ int Y,
    _In_ int cx,
    _In_ int cy,
    _In_ UINT uFlags);

typedef HWND (WINAPI *CreateWindowExA_T)(
	_In_ DWORD dwExStyle,
	_In_opt_ LPCSTR lpClassName,
	_In_opt_ LPCSTR lpWindowName,
	_In_ DWORD dwStyle,
	_In_ int X,
	_In_ int Y,
	_In_ int nWidth,
	_In_ int nHeight,
	_In_opt_ HWND hWndParent,
	_In_opt_ HMENU hMenu,
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ LPVOID lpParam);

namespace Window
{
	extern bool BorderlessUpscaling;
	extern bool ForceBorderless;
	extern bool CursorFix;

    extern void OnSwapChainCreate(HWND hWnd);
	void InstallHooksIfLoaded();
}