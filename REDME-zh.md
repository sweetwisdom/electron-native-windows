







# BrowserWindowTool - 在Electron中嵌入Windows应用程序窗口

## 项目概述

BrowserWindowTool是一个Electron项目，旨在在Electron应用程序窗口中嵌入其他Windows应用程序（如记事本、画图、计算器等）。通过使用Node.js原生模块，该项目实现了在Electron窗口内无缝嵌入和控制其他Windows应用程序窗口的功能。

## 项目结构

```
├── binding.gyp                  # Node.js原生模块构建配置
├── electron-example             # Electron应用
│   ├── blank.html               # 嵌入窗口的空白模板
│   ├── index.html               # 主界面
│   ├── main.js                  # Electron主进程
│   ├── preload.js               # 预加载脚本
│   └── render.js                # 渲染进程脚本
└── src                          # 原生模块源代码
    ├── main.cc                  # N-API模块入口
    ├── WindowManager.cc         # 窗口管理实现
    └── WindowManager.h          # 窗口管理头文件
```

```tip

├── release/BrowserWindowTool.node # 为 windows下 nodegyp 的构建产物

如果没有vs环境，删除binding.gyp文件，将`release/BrowserWindowTool.node`复制到`/build/Release/BrowserWindowTool.node`

```


## 效果展示

视频效果

<video width="900px" height="auto" controls>
<source src="./.imgs/demo.mp4" type="video/mp4">
</video>

集成网页云音乐

<img src=".imgs/image-20251105105003418.png" alt="image-20251105105003418" style="zoom:50%;" />



集成notepad++

<img src=".imgs/image-20251105104040297.png" alt="image-20251105104040297" style="zoom:50%;" />

## 已知bug

某些程序可能无法集成

<img src=".imgs/image-20251105104548437.png" alt="image-20251105104548437" style="zoom:50%;" />

## 核心原理

### 1. 为什么需要原生模块？

Electron基于Chromium和Node.js，它本身不提供直接嵌入其他Windows应用程序窗口的能力。Chromium的渲染引擎和窗口管理机制与Windows原生窗口系统不兼容，因此我们需要使用原生模块来实现这一功能。

### 2. 嵌入技术原理

BrowserWindowTool的核心原理是通过以下步骤实现的：

1. **创建容器窗口**：在Electron窗口中创建一个透明的子窗口（容器窗口），作为嵌入其他应用程序窗口的"画布"。

2. **启动目标进程**：使用Windows API启动目标应用程序进程。

3. **查找目标窗口**：通过枚举窗口，找到目标应用程序的主窗口。

4. **嵌入目标窗口**：将目标应用程序窗口的父窗口设置为容器窗口，从而将目标窗口嵌入到容器窗口中。

5. **调整窗口大小和位置**：根据容器窗口的大小和位置，调整嵌入窗口的大小和位置。

### 3. 关键技术点

#### a. 禁用GPU加速

Electron默认启用GPU加速，这会导致渲染冲突。因此，项目在启动时添加了以下命令行参数：

```javascript
app.commandLine.appendSwitch("in-process-gpu");
app.commandLine.appendSwitch("disable-gpu-sandbox");
app.commandLine.appendSwitch("disable-direct-composition");
app.commandLine.appendSwitch("disable-gpu");
app.commandLine.appendSwitch("no-sandbox");
```

这些参数禁用了GPU加速，避免了渲染冲突。

#### b. 原生模块与Node.js的交互

项目使用Node.js的N-API（Node API）创建了一个原生模块（BrowserWindowTool），用于处理Windows API调用。这个模块提供了以下关键功能：

- `createEmbeddedWindow`: 创建嵌入窗口
- `updateWindow`: 更新嵌入窗口
- `destroyWindow`: 销毁嵌入窗口
- `getAllWindowIds`: 获取所有窗口ID
- `cleanupAll`: 清理所有窗口

#### c. 窗口嵌入实现

在`WindowManager.cc`中，关键的嵌入逻辑如下：

```cpp
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
    
    // 设置窗口样式，使其成为子窗口
    LONG_PTR style = GetWindowLongPtr(targetWindow, GWL_STYLE);
    style = (style & ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME)) | WS_CHILD;
    SetWindowLongPtr(targetWindow, GWL_STYLE, style);
    
    // 设置窗口扩展样式
    LONG_PTR exStyle = GetWindowLongPtr(targetWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLongPtr(targetWindow, GWL_EXSTYLE, exStyle);
    
    // 设置父窗口
    SetParent(targetWindow, containerWindow);
    
    // 调整窗口大小
    RECT rect;
    GetClientRect(containerWindow, &rect);
    SetWindowPos(targetWindow, HWND_TOP, 0, 0, 
                rect.right - rect.left, rect.bottom - rect.top, 
                SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    
    ::ShowWindow(targetWindow, SW_SHOW);
    ::BringWindowToTop(targetWindow);
    ::UpdateWindow(targetWindow);
    
    return targetWindow;
}
```

这段代码的关键点是：
- 通过`EnumWindows`枚举窗口，找到目标应用程序的主窗口
- 修改目标窗口的样式，使其成为子窗口（`WS_CHILD`）
- 使用`SetParent`将目标窗口的父窗口设置为容器窗口
- 调整目标窗口的大小以适应容器窗口

#### d. 处理窗口事件

在`ContainerWndProc`中，处理了窗口大小变化事件：

```cpp
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
```

当容器窗口大小变化时，调整嵌入窗口的大小以适应容器窗口。


## 关键交互流程

1. **Electron主进程创建容器窗口**:
   - 创建Electron窗口
   - 加载空白HTML页面（blank.html）
   - 获取窗口的原生句柄

2. **调用原生模块创建嵌入窗口**:
   - 通过IPC将窗口句柄和参数传递给原生模块
   - 原生模块创建容器窗口（WindowManager）
   - 启动目标应用程序进程
   - 查找目标窗口并嵌入

3. **窗口管理**:
   - 通过IPC接口实现窗口的更新、显示、隐藏和销毁
   - 原生模块维护窗口状态和映射关系

## 技术挑战与解决方案

### 1. 窗口层级问题

**问题**: Electron窗口和目标应用程序窗口的层级关系混乱。

**解决方案**:
- 通过`SetWindowPos`调整窗口层级
- 使用`BringWindowToTop`确保嵌入窗口在最上层

### 2. 窗口样式兼容性

**问题**: 目标应用程序窗口可能带有标题栏、边框等，与嵌入环境不兼容。

**解决方案**:
- 修改窗口样式，移除标题栏和边框
- 使用`WS_CHILD`将窗口设置为子窗口

### 3. 窗口大小自适应

**问题**: 当容器窗口大小变化时，嵌入窗口需要相应调整。

**解决方案**:
- 在容器窗口的`WM_SIZE`消息处理中调整嵌入窗口大小
- 通过`SetWindowPos`更新嵌入窗口的尺寸

## 注意事项

1. **仅支持Windows**：该技术依赖于Windows API，因此仅在Windows系统上工作。

2. **目标应用程序要求**：目标应用程序必须是Windows原生应用程序，不支持基于Web的浏览器应用。

3. **窗口样式问题**：某些应用程序的窗口样式可能需要特殊处理，以确保嵌入后显示正常。

4. **性能考虑**：嵌入大量窗口可能会影响性能，特别是对于资源密集型应用程序。

5. **安全风险**：禁用GPU加速和沙盒可能会降低安全性，因此建议在受信任的环境中使用。

### 参考

[xland/ElectronGrpc: Electron comunicate with native process by grpc](https://github.com/xland/ElectronGrpc)
