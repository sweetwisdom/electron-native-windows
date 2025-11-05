#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <windows.h>
#include <string>
#include <map>
#include <memory>
#include <vector>

struct EmbeddedProcess
{
    PROCESS_INFORMATION processInfo;
    HWND embedWindow;
    HWND targetWindow;
    std::wstring processPath;
    std::wstring arguments;
    bool isRunning;
    DWORD processId;
};

class WindowManager
{
public:
    static WindowManager &Instance();

    std::string CreateEmbeddedWindow(
        HWND parentWindow,
        const std::wstring &exePath,
        const std::wstring &args,
        int x, int y, int width, int height);

    bool UpdateWindow(const std::string &id, int x, int y, int width, int height);
    bool DestroyWindow(const std::string &id);
    bool ShowWindow(const std::string &id, bool show);
    std::vector<std::string> GetAllWindowIds();
    void CleanupAll();

private:
    WindowManager();
    ~WindowManager();
    WindowManager(const WindowManager &) = delete;
    WindowManager &operator=(const WindowManager &) = delete;

    bool PrepareParentWindow(HWND parentWindow);
    HWND CreateContainerWindow(HWND parentWindow, int x, int y, int width, int height);
    bool LaunchProcess(const std::wstring &exePath, const std::wstring &args, PROCESS_INFORMATION &processInfo);
    HWND FindAndEmbedWindow(DWORD processId, HWND containerWindow);
    std::string GenerateId();

    static LRESULT CALLBACK ContainerWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    std::map<std::string, std::shared_ptr<EmbeddedProcess>> processes_;
    ATOM containerClassAtom_;
    int nextId_;
};

#endif