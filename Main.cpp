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

std::filesystem::path GetRtssCliPath()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);

    return std::filesystem::path(path).parent_path()
        / L"tools" / L"rtss-cli.exe";
}

bool RunRtssCli(const std::vector<std::wstring>& args,
    std::wstring& output)
{
    const auto exe = GetRtssCliPath();

    if (!std::filesystem::exists(exe))
        return false;

    std::wstring command = L"\"" + exe.wstring() + L"\"";

    for (const auto& arg : args)
        command += L" \"" + arg + L"\"";

    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');

    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readPipe{}, writePipe{};

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
        return false;

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};

    BOOL ok = CreateProcessW(
        exe.c_str(), buffer.data(),
        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, exe.parent_path().c_str(), &si, &pi);

    CloseHandle(writePipe);

    if (!ok)
    {
        CloseHandle(readPipe);
        return false;
    }

    output.clear();
    char chunk[1024];
    DWORD bytes{};

    while (ReadFile(readPipe, chunk, sizeof(chunk), &bytes, nullptr)
        && bytes > 0)
    {
        for (DWORD i = 0; i < bytes; ++i)
            output += static_cast<unsigned char>(chunk[i]);
    }

    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode{};
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

bool SetFpsLimiter(bool enabled)
{
    std::wstring output;
    return RunRtssCli(
        { L"limiter:set", enabled ? L"1" : L"0" }, output);
}

bool GetFpsLimiter(bool& enabled)
{
    std::wstring output;

    if (!RunRtssCli({ L"limiter:get" }, output))
        return false;

    while (!output.empty() &&
        (output.back() == L'\r' || output.back() == L'\n' ||
            output.back() == L' ' || output.back() == L'\t'))
        output.pop_back();

    if (output == L"1")
        enabled = true;
    else if (output == L"0")
        enabled = false;
    else
        return false;

    return true;
}


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
            SetFpsLimiter(true);
            break;

        case ID_HOTKEY_F2:
            SetFpsLimiter(false);
            break;

        case ID_HOTKEY_F3:
            SetFpsLimiter(false);
            DestroyWindow(hwnd);
            break;
        }
        return 0;

        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT)
        {
            SetFpsLimiter(false);
            DestroyWindow(hwnd);
        }
        return 0;


    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &nid);
        SetFpsLimiter(false);
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