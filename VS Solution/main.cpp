#include "resource.h"
#include <Windows.h>
#include <Windowsx.h>

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

import Utility;

#define MAX_LOADSTRING 100

HINSTANCE g_hInstance;                       // current instance
LPCWSTR szWindowClass { L"WindowClass" };    // the main window class name
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK TestDialogProc(HWND, UINT, WPARAM, LPARAM);

GDIUT::CSplitter g_Splitter;
GDIUT::CDynLayout g_DynLayout;
GDIUT::CLinkCtrl g_Link1;
GDIUT::CLinkCtrl g_Link2;

WPARAM RunMainWnd(HINSTANCE hInstance, int nCmdShow) {
	MyRegisterClass(hInstance);

	if (!InitInstance(hInstance, nCmdShow)) {
		return FALSE;
	}

	const auto hAccelTable = LoadAcceleratorsW(hInstance, MAKEINTRESOURCE(IDC_UTILITY));
	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0)) {
		if (!TranslateAcceleratorW(msg.hwnd, hAccelTable, &msg)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	return (int)msg.wParam;
}

int APIENTRY wWinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
	[[maybe_unused]] LPWSTR lpCmdLine, [[maybe_unused]] int nCmdShow) {
	g_hInstance = hInstance;
	//RunMainWnd(g_hInstance, nCmdShow);
	DialogBoxParamW(g_hInstance, MAKEINTRESOURCE(IDD_TEST), nullptr, TestDialogProc, 0);

	return 0;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_UTILITY));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_UTILITY);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	const auto hWnd = CreateWindowW(szWindowClass, L"Title", WS_OVERLAPPEDWINDOW,
	   CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd) {
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		switch (wmId) { //Parse the menu selections:
		case IDM_ABOUT:
			DialogBoxParamW(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About, 0);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		::BeginPaint(hWnd, &ps);

		::EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, [[maybe_unused]] LPARAM lParam)
{
	switch (message) {
	case WM_INITDIALOG:
		g_Link1.Initialize(hDlg, IDC_STATIC_COPYRIGHT, L"https://google.com");
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

INT_PTR CALLBACK TestDialogProc(HWND hDlg, UINT message, WPARAM wParam, [[maybe_unused]] LPARAM lParam)
{
	switch (message) {
	case WM_DPICHANGED:
		g_DynLayout.Enable(true);
		g_Link1.WMDPIChanged();
		g_Link2.WMDPIChanged();
		return 0;
	case WM_GETDPISCALEDSIZE:
		g_DynLayout.Enable(false);
		return 0;
	case WM_INITDIALOG:
		g_DynLayout.Initialize(hDlg);
		g_DynLayout.AddItem(IDC_BUTTON_TEST, GDIUT::CDynLayout::MoveNone(), GDIUT::CDynLayout::SizeHorzAndVert(50, 100));
		g_DynLayout.AddItem(IDC_LIST, GDIUT::CDynLayout::MoveHorz(50), GDIUT::CDynLayout::SizeHorzAndVert(50, 100));
		g_DynLayout.AddItem(IDOK, GDIUT::CDynLayout::MoveHorzAndVert(100, 100), GDIUT::CDynLayout::SizeNone());
		g_DynLayout.AddItem(IDCANCEL, GDIUT::CDynLayout::MoveHorzAndVert(100, 100), GDIUT::CDynLayout::SizeNone());
		g_DynLayout.Enable(true);

		g_Splitter.Initialize(hDlg, IDC_LIST, GDIUT::CSplitter::EAnchorSide::SIDE_LEFT);
		g_Splitter.SetEdges(30, 400);
		g_Splitter.AddItem(IDC_BUTTON_TEST, true);

		g_Link1.Initialize(hDlg, IDC_STATIC1, L"https://google.com");
		g_Link2.Initialize(hDlg, IDC_STATIC2);

		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
			::EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}