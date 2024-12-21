#include "pch.h"

// Windows headers in correct order
#include <windows.h>
#include <objidl.h>    // For IStream
#include <gdiplus.h>   // Must come after objidl.h
#include <tchar.h>
#include <memory>
#include <type_traits>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")

enum ZBID
{
    ZBID_DEFAULT = 0,
    ZBID_DESKTOP = 1,
    ZBID_UIACCESS = 2,
    ZBID_IMMERSIVE_IHM = 3,
    ZBID_IMMERSIVE_NOTIFICATION = 4,
    ZBID_IMMERSIVE_APPCHROME = 5,
    ZBID_IMMERSIVE_MOGO = 6,
    ZBID_IMMERSIVE_EDGY = 7,
    ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
    ZBID_IMMERSIVE_INACTIVEDOCK = 9,
    ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
    ZBID_IMMERSIVE_ACTIVEDOCK = 11,
    ZBID_IMMERSIVE_BACKGROUND = 12,
    ZBID_IMMERSIVE_SEARCH = 13,
    ZBID_GENUINE_WINDOWS = 14,
    ZBID_IMMERSIVE_RESTRICTED = 15,
    ZBID_SYSTEM_TOOLS = 16,
    ZBID_LOCK = 17,
    ZBID_ABOVELOCK_UX = 18,
};

const TCHAR* WindowTitle = _T("RustDeskLoadingWindow");
const TCHAR* ClassName = _T("RustDeskLoadingWindowClass");

typedef HWND(WINAPI* CreateWindowInBand)(_In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band);

HWND g_hwnd;
float g_angle = 0.0f;
ULONG_PTR g_gdiplusToken;

// Function declarations
VOID OnPaintGdiPlus(HWND hwnd, HDC hdc);
BOOL IsWindowsVersionOrGreater(DWORD os_major, DWORD os_minor, DWORD build_number, WORD service_pack_major, WORD service_pack_minor);
void ShowErrorMsg(const TCHAR* caption);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 16, NULL); // ~60 FPS
        break;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    case WM_WINDOWPOSCHANGING:
        return 0;

    case WM_CLOSE:
        {
            HANDLE myself = OpenProcess(PROCESS_ALL_ACCESS, false, GetCurrentProcessId());
            TerminateProcess(myself, 0);
            return TRUE;
        }

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (hdc)
            {
                OnPaintGdiPlus(hwnd, hdc);
                EndPaint(hwnd, &ps);
            }
        }
        break;

    case WM_TIMER:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

VOID OnPaintGdiPlus(HWND hwnd, HDC hdc)
{
    if (!hdc)
        return;

    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int width = rcClient.right - rcClient.left;
    int height = rcClient.bottom - rcClient.top;

    // Create graphics object with double buffering
    Gdiplus::Bitmap buffer(width, height);
    Gdiplus::Graphics graphics(&buffer);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    // Semi-transparent black background
    Gdiplus::SolidBrush backgroundBrush(Gdiplus::Color(180, 0, 0, 0));
    graphics.FillRectangle(&backgroundBrush, 0, 0, width, height);

    // Center point for the loading circle
    float centerX = width / 2.0f;
    float centerY = height / 2.0f - 50.0f;
    float radius = 40.0f;

    // Draw loading circle segments
    for (int i = 0; i < 12; i++)
    {
        float angle = g_angle + (i * 30.0f);
        float alpha = 255.0f - (i * (255.0f / 12.0f));

        Gdiplus::Pen pen(Gdiplus::Color(static_cast<BYTE>(alpha), 255, 255, 255), 4.0f);
        
        float startAngle = angle;
        float sweepAngle = 20.0f;

        graphics.DrawArc(&pen, 
            centerX - radius, 
            centerY - radius,
            radius * 2,
            radius * 2,
            startAngle,
            sweepAngle);
    }

    // Draw text
    Gdiplus::FontFamily fontFamily(L"Segoe UI");
    Gdiplus::Font font(&fontFamily, 16.0f);
    Gdiplus::StringFormat format;
    format.SetAlignment(Gdiplus::StringAlignmentCenter);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 255, 255, 255));
    
    const WCHAR text[] = L"Please wait while the system is processing...";
    Gdiplus::RectF layoutRect(0, centerY + radius + 20.0f, (Gdiplus::REAL)width, 50.0f);
    
    graphics.DrawString(text, -1, &font, layoutRect, &format, &textBrush);

    // Copy buffer to screen
    Gdiplus::Graphics screenGraphics(hdc);
    screenGraphics.DrawImage(&buffer, 0, 0);

    // Update angle for animation
    g_angle += 5.0f;
    if (g_angle >= 360.0f)
        g_angle = 0.0f;
}

HWND CreateWin(HMODULE hModule, UINT zbid)
{
    HINSTANCE hInstance = hModule;
    WNDCLASSEX wndClass = { 0 };

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.lpfnWndProc = WindowProc;
    wndClass.hInstance = hInstance;
    wndClass.lpszClassName = ClassName;
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.hCursor = LoadCursor(0, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    auto res = RegisterClassEx(&wndClass);
    if (res == 0)
    {
        ShowErrorMsg(_T("RegisterClassEx"));
        return nullptr;
    }

    const auto hpath = LoadLibrary(_T("user32.dll"));
    if (hpath == 0)
    {
        ShowErrorMsg(_T("LoadLibrary user32.dll"));
        return nullptr;
    }

    const auto pCreateWindowInBand = CreateWindowInBand(GetProcAddress(hpath, "CreateWindowInBand"));
    if (!pCreateWindowInBand)
    {
        ShowErrorMsg(_T("GetProcAddress CreateWindowInBand"));
        return nullptr;
    }

    HWND hwnd = pCreateWindowInBand(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        res,
        NULL,
        WS_POPUP,
        0, 0, 0, 0,
        NULL,
        NULL,
        wndClass.hInstance,
        NULL,
        zbid);

    if (!hwnd)
    {
        ShowErrorMsg(_T("CreateWindowInBand"));
        return nullptr;
    }

    SetWindowText(hwnd, WindowTitle);

    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(hmon, &mi);

    SetWindowPos(
        hwnd,
        nullptr,
        mi.rcMonitor.left,
        mi.rcMonitor.top,
        mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top,
        SWP_SHOWWINDOW | SWP_NOZORDER
    );

    SetWindowLong(
        hwnd,
        GWL_EXSTYLE,
        GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE
    );

    // Set window transparency
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    return hwnd;
}

void ShowErrorMsg(const TCHAR* caption)
{
    DWORD code = GetLastError();
    TCHAR msg[256] = { 0 };
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msg, (sizeof(msg) / sizeof(msg[0])), NULL);

#ifdef WINDOWINJECTION_EXPORTS
    TCHAR buf[1024] = { 0 };
    _sntprintf_s(buf, sizeof(buf) / sizeof(buf[0]), _TRUNCATE, _T("%s, code 0x%x"), msg, code);
    MessageBox(NULL, buf, caption, 0);
#else
    _tprintf(_T("%s: %s, code 0x%x\n"), caption, msg, code);
#endif
}

DWORD WINAPI MainThread(LPVOID lpParam)
{
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

#ifdef WINDOWINJECTION_EXPORTS
    g_hwnd = CreateWin(NULL, ZBID_ABOVELOCK_UX);
#else
    g_hwnd = CreateWin(NULL, ZBID_DESKTOP);
#endif

    if (!g_hwnd)
    {
        return 0;
    }

    // Process messages
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup GDI+
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return 0;
}

#ifdef WINDOWINJECTION_EXPORTS

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
    switch (ulReasonForCall)
    {
    case DLL_PROCESS_ATTACH:
        CreateThread(nullptr, 0, MainThread, hModule, NULL, NULL);
        break;
    case DLL_PROCESS_DETACH:
        if (g_hwnd)
        {
            PostMessage(g_hwnd, WM_CLOSE, NULL, NULL);
        }
        break;
    }
    return TRUE;
}

#else

int main(int argc, char* argv[])
{
    HMODULE hInstance = nullptr;
    BOOL result = GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<char*>(&DefWindowProc),
        &hInstance);

    if (FALSE == result)
    {
        printf("Failed to GetModuleHandleExA, 0x%x\n", GetLastError());
        return 0;
    }

    return MainThread(hInstance);
}

#endif

// Windows version check helper
BOOL IsWindowsVersionOrGreater(
    DWORD os_major,
    DWORD os_minor,
    DWORD build_number,
    WORD service_pack_major,
    WORD service_pack_minor)
{
    OSVERSIONINFOEX osvi;
    DWORDLONG condition_mask = 0;
    int op = VER_GREATER_EQUAL;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = os_major;
    osvi.dwMinorVersion = os_minor;
    osvi.dwBuildNumber = build_number;
    osvi.wServicePackMajor = service_pack_major;
    osvi.wServicePackMinor = service_pack_minor;

    VER_SET_CONDITION(condition_mask, VER_MAJORVERSION, op);
    VER_SET_CONDITION(condition_mask, VER_MINORVERSION, op);
    VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMAJOR, op);
    VER_SET_CONDITION(condition_mask, VER_SERVICEPACKMINOR, op);

    return VerifyVersionInfo(
        &osvi,
        VER_MAJORVERSION | VER_MINORVERSION |
        VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
        condition_mask);
}