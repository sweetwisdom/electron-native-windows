#include "WindowManager.h"
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <stdexcept>

WindowManager &WindowManager::Instance()
{
    static WindowManager instance;
    return instance;
}

WindowManager::WindowManager() : containerClassAtom_(0), nextId_(1)
{
    WNDCLASSEXW wcx = {};
    wcx.cbSize = sizeof(wcx);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.hInstance = GetModuleHandle(NULL);
    wcx.lpfnWndProc = &WindowManager::ContainerWndProc;
    wcx.lpszClassName = L"EmbeddedWindowContainer";
    wcx.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);

    containerClassAtom_ = RegisterClassExW(&wcx);
}

WindowManager::~WindowManager()
{
    CleanupAll();
    if (containerClassAtom_)
    {
        UnregisterClass(MAKEINTATOM(containerClassAtom_), GetModuleHandle(NULL));
    }
}

std::string WindowManager::GenerateId()
{
    std::ostringstream oss;
    oss << "embedded_" << std::setfill('0') << std::setw(6) << nextId_++;
    return oss.str();
}

bool WindowManager::PrepareParentWindow(HWND parentWindow)
{
    if (!IsWindow(parentWindow))
    {
        return false;
    }

    HWND hwndD3D = FindWindowEx(parentWindow, NULL, L"Intermediate D3D Window", NULL);

    if (hwndD3D)
    {
        LONG_PTR style = GetWindowLongPtr(hwndD3D, GWL_STYLE);
        if ((style & WS_CLIPSIBLINGS) == 0)
        {
            style |= WS_CLIPSIBLINGS;
            SetWindowLongPtr(hwndD3D, GWL_STYLE, style);
        }
    }

    LONG_PTR style = GetWindowLongPtr(parentWindow, GWL_STYLE);
    if ((style & WS_CLIPCHILDREN) == 0)
    {
        style |= WS_CLIPCHILDREN;
        SetWindowLongPtr(parentWindow, GWL_STYLE, style);
    }

    return true;
}

HWND WindowManager::CreateContainerWindow(HWND parentWindow, int x, int y, int width, int height)
{
    if (!containerClassAtom_)
    {
        return NULL;
    }

    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    HWND hwnd = CreateWindowExW(
        0,
        MAKEINTATOM(containerClassAtom_),
        L"Container",
        style,
        x, y, width, height,
        parentWindow,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    return hwnd;
}

bool WindowManager::LaunchProcess(const std::wstring &exePath, const std::wstring &args, PROCESS_INFORMATION &processInfo)
{
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    ZeroMemory(&processInfo, sizeof(processInfo));

    std::wstring cmdLine = L"\"" + exePath + L"\"";
    if (!args.empty())
    {
        cmdLine += L" " + args;
    }

    std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
    cmdLineBuf.push_back(L'\0');

    BOOL result = CreateProcessW(
        exePath.c_str(),
        cmdLineBuf.data(),
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &processInfo);

    return result != 0;
}

struct EnumWindowsData
{
    DWORD processId;
    HWND targetWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumWindowsData *data = reinterpret_cast<EnumWindowsData *>(lParam);
    DWORD windowProcessId;
    GetWindowThreadProcessId(hwnd, &windowProcessId);

    if (windowProcessId == data->processId && IsWindowVisible(hwnd))
    {
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);

        std::wstring classNameStr(className);
        if (classNameStr != L"ConsoleWindowClass" &&
            classNameStr != L"IME" &&
            GetWindow(hwnd, GW_OWNER) == NULL)
        {
            data->targetWindow = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

HWND WindowManager::FindAndEmbedWindow(DWORD processId, HWND containerWindow)
{
    HWND targetWindow = NULL;

    for (int i = 0; i < 50; ++i)
    {
        EnumWindowsData data = {processId, NULL};
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(static_cast<void *>(&data)));

        if (data.targetWindow)
        {
            targetWindow = data.targetWindow;
            break;
        }

        Sleep(100);
    }

    if (!targetWindow || !IsWindow(targetWindow))
    {
        return NULL;
    }

    LONG_PTR style = GetWindowLongPtr(targetWindow, GWL_STYLE);
    style = (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME)) | WS_CHILD;
    SetWindowLongPtr(targetWindow, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(targetWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(targetWindow, GWL_EXSTYLE, exStyle);

    SetParent(targetWindow, containerWindow);

    RECT rect;
    GetClientRect(containerWindow, &rect);
    SetWindowPos(targetWindow, HWND_TOP, 0, 0,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    ::ShowWindow(targetWindow, SW_SHOW);
    ::BringWindowToTop(targetWindow);
    ::UpdateWindow(targetWindow);
    ::SetWindowPos(containerWindow, HWND_TOPMOST, 0, 0,
                   rect.right - rect.left, rect.bottom - rect.top,
                   SWP_SHOWWINDOW | SWP_FRAMECHANGED);

    return targetWindow;
}

std::string WindowManager::CreateEmbeddedWindow(
    HWND parentWindow,
    const std::wstring &exePath,
    const std::wstring &args,
    int x, int y, int width, int height)
{
    if (!IsWindow(parentWindow))
    {
        throw std::runtime_error("Invalid parent window handle");
    }

    if (exePath.empty())
    {
        throw std::runtime_error("Executable path cannot be empty");
    }

    DWORD fileAttr = GetFileAttributesW(exePath.c_str());
    if (fileAttr == INVALID_FILE_ATTRIBUTES)
    {
        throw std::runtime_error("Executable file not found");
    }

    if (!PrepareParentWindow(parentWindow))
    {
        throw std::runtime_error("Failed to prepare parent window");
    }

    HWND containerWindow = CreateContainerWindow(parentWindow, x, y, width, height);
    if (!containerWindow)
    {
        throw std::runtime_error("Failed to create container window");
    }

    auto process = std::make_shared<EmbeddedProcess>();
    if (!LaunchProcess(exePath, args, process->processInfo))
    {
        ::DestroyWindow(containerWindow);
        throw std::runtime_error("Failed to launch process");
    }

    HWND targetWindow = FindAndEmbedWindow(process->processInfo.dwProcessId, containerWindow);
    if (!targetWindow)
    {
        TerminateProcess(process->processInfo.hProcess, 0);
        CloseHandle(process->processInfo.hProcess);
        CloseHandle(process->processInfo.hThread);
        ::DestroyWindow(containerWindow);
        throw std::runtime_error("Failed to find or embed target window");
    }

    process->embedWindow = containerWindow;
    process->targetWindow = targetWindow;
    process->processPath = exePath;
    process->arguments = args;
    process->isRunning = true;
    process->processId = process->processInfo.dwProcessId;

    // 确保窗口显示
    ::ShowWindow(containerWindow, SW_SHOW);
    ::ShowWindow(targetWindow, SW_SHOW);
    ::UpdateWindow(containerWindow);
    ::UpdateWindow(targetWindow);

    std::string id = GenerateId();
    processes_[id] = process;

    return id;
}

bool WindowManager::UpdateWindow(const std::string &id, int x, int y, int width, int height)
{
    auto it = processes_.find(id);
    if (it == processes_.end())
    {
        return false;
    }

    auto &process = it->second;
    if (!process->isRunning || !IsWindow(process->embedWindow))
    {
        return false;
    }

    SetWindowPos(process->embedWindow, NULL, x, y, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    if (IsWindow(process->targetWindow))
    {
        RECT rect;
        GetClientRect(process->embedWindow, &rect);
        SetWindowPos(process->targetWindow, NULL, 0, 0,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return true;
}

bool WindowManager::ShowWindow(const std::string &id, bool show)
{
    auto it = processes_.find(id);
    if (it == processes_.end())
    {
        return false;
    }

    auto &process = it->second;
    if (!IsWindow(process->embedWindow))
    {
        return false;
    }

    ::ShowWindow(process->embedWindow, show ? SW_SHOW : SW_HIDE);
    if (IsWindow(process->targetWindow))
    {
        ::ShowWindow(process->targetWindow, show ? SW_SHOW : SW_HIDE);
    }

    return true;
}

bool WindowManager::DestroyWindow(const std::string &id)
{
    auto it = processes_.find(id);
    if (it == processes_.end())
    {
        return false;
    }

    auto process = it->second;
    process->isRunning = false;

    if (process->processInfo.hProcess)
    {
        TerminateProcess(process->processInfo.hProcess, 0);
        WaitForSingleObject(process->processInfo.hProcess, 2000);
        CloseHandle(process->processInfo.hProcess);
        CloseHandle(process->processInfo.hThread);
    }

    if (IsWindow(process->embedWindow))
    {
        ::DestroyWindow(process->embedWindow);
    }

    processes_.erase(it);
    return true;
}

std::vector<std::string> WindowManager::GetAllWindowIds()
{
    std::vector<std::string> ids;
    for (const auto &pair : processes_)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

void WindowManager::CleanupAll()
{
    auto ids = GetAllWindowIds();
    for (const auto &id : ids)
    {
        DestroyWindow(id);
    }
}

LRESULT CALLBACK WindowManager::ContainerWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_SIZE:
    {
        HWND childWindow = GetWindow(hwnd, GW_CHILD);
        if (childWindow)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            SetWindowPos(childWindow, NULL, 0, 0,
                         rect.right - rect.left, rect.bottom - rect.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_NCCALCSIZE:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}