#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <shellapi.h>
#pragma comment(lib, "Shell32.lib")

//icon
#include "resource.h"
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001

//define the buttons
#define ID_HOTKEY_F1 2001
#define ID_HOTKEY_F2 2002
#define ID_HOTKEY_F3 2003

NOTIFYICONDATAW nid = {};




//handle rtss-cli   





LRESULT CALLBACK WndProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {


    case WM_TRAYICON:
    {
        // With NOTIFYICON_VERSION_4:
        // event is in LOWORD(lParam)
        // icon ID is in HIWORD(lParam)
        switch (LOWORD(lParam))
        {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
        {
            HMENU menu = CreatePopupMenu();

            AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

            POINT pt;
            GetCursorPos(&pt);

            SetForegroundWindow(hwnd);

            UINT cmd = TrackPopupMenu(
                menu,
                TPM_RIGHTBUTTON |
                TPM_BOTTOMALIGN |
                TPM_RETURNCMD,
                pt.x, pt.y, 0, hwnd, nullptr);

            DestroyMenu(menu);

            // Required for proper menu dismissal behavior.
            PostMessageW(hwnd, WM_NULL, 0, 0);

            if (cmd == ID_TRAY_EXIT)
                DestroyWindow(hwnd);

            return 0;
        }

        case WM_LBUTTONDBLCLK:
            MessageBoxW(
                nullptr,
                L" Activate with Shift F1 \r\n Deactivate with Shift F2, \r\n Deactivate and Close with Exit or Shift F3!",
                L"RFPS Limiter",
                MB_OK | MB_ICONINFORMATION);
            return 0;
        }

    case WM_HOTKEY:
        switch (wParam)
        {
        case ID_HOTKEY_F1:
            MessageBoxW(
                nullptr, L"HELLO",
                L"My Tray App", MB_OK);
            break;

        case ID_HOTKEY_F2:
            MessageBoxW(
                nullptr, L"GOOD BYE",
                L"My Tray App", MB_OK);
            break;

        case ID_HOTKEY_F3:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT)
        {
            DestroyWindow(hwnd);
        }
        return 0;


    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ PWSTR pCmdLine,
    _In_ int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"TrayAppWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);


    // Create a hidden message-only window.
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"Tray App",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, hInstance, nullptr);

    if (!hwnd)
        return 1;


    //set the icon
    HICON hIcon = (HICON)LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_ICON1),
        IMAGE_ICON,
        32, 32,
        LR_DEFAULTCOLOR
    );

    HICON hSmallIcon = (HICON)LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_ICON1),
        IMAGE_ICON,
        16, 16,
        LR_DEFAULTCOLOR
    );



    //register the keys
    RegisterHotKey(hwnd, ID_HOTKEY_F1,
        MOD_SHIFT | MOD_NOREPEAT, VK_F1);

    RegisterHotKey(hwnd, ID_HOTKEY_F2,
        MOD_SHIFT | MOD_NOREPEAT, VK_F2);

    RegisterHotKey(hwnd, ID_HOTKEY_F3,
        MOD_SHIFT | MOD_NOREPEAT, VK_F3);

    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;

    SendMessageW(hwnd, WM_SETICON, ICON_BIG,
        (LPARAM)hIcon);

    SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
        (LPARAM)hSmallIcon);

    lstrcpynW(nid.szTip, L"RFPS Limiter",
        ARRAYSIZE(nid.szTip));

    if (!Shell_NotifyIconW(NIM_ADD, &nid))
        return 1;

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}