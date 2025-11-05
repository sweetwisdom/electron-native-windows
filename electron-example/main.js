const { app, BrowserWindow, ipcMain } = require("electron");
const path = require("path");

// 关键：禁用GPU加速相关选项，避免渲染冲突
app.commandLine.appendSwitch("in-process-gpu");
app.commandLine.appendSwitch("disable-gpu-sandbox");
app.commandLine.appendSwitch("disable-direct-composition"); // 禁用D3D composition
app.commandLine.appendSwitch("disable-gpu"); // 完全禁用GPU加速
app.commandLine.appendSwitch("no-sandbox"); // 禁用沙盒以减少冲突
app.commandLine.appendSwitch("disable-backgrounding-occluded-windows", "false");
app.commandLine.appendSwitch("disable-renderer-backgrounding");

let mainWindow;
let nativeAddon;
// 存储窗口ID和对应的BrowserWindow映射
const embeddedWindows = new Map();

// 加载原生模块
try {
  nativeAddon = require("../build/Release/BrowserWindowTool.node");
  console.log("Native addon loaded successfully");
} catch (error) {
  console.error("Failed to load native addon:", error);
  nativeAddon = null;
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      preload: path.join(__dirname, "preload.js"),
    },
    backgroundColor: "#f0f0f0",
    transparent: false,
    frame: true,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      preload: path.join(__dirname, "preload.js"),
      hardwareAcceleration: false,
      offscreen: false,
      backgroundThrottling: false,
    },
  });

  mainWindow.loadFile("index.html");

  mainWindow.on("closed", () => {
    // 清理所有嵌入的窗口
    if (nativeAddon) {
      try {
        nativeAddon.cleanupAll();
      } catch (error) {
        console.error("Error cleaning up:", error);
      }
    }
    mainWindow = null;
  });
}
function UseWindowConfig() {
  const options  = {width: 1200, height: 700}
  console.log('current width & height：', options)
  return {
    titleBarHeight: 55,
    menuSidebarWidth: 60,
    windowWidth: options.width || 1200,
    windowHeight: options.height || 700,
    embeddedX: 2, // 对应 .content-area 的左侧，无偏移
    embeddedY: 60, // 对应 .title-bar 下方，从 40px 开始
  };
}

// 创建嵌入窗口
ipcMain.handle("create-embedded-window", async (event, options) => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    // 计算窗口在屏幕上的居中位置
    const { screen } = require("electron");
    const primaryDisplay = screen.getPrimaryDisplay();
    const { width: screenWidth, height: screenHeight } =
      primaryDisplay.workAreaSize;
    // const windowWidth = options.width || 1200;
    // const windowHeight = options.height || 700;
    const {
      windowWidth,
      windowHeight,
      titleBarHeight,
      menuSidebarWidth,
      embeddedX,
      embeddedY,
    } = UseWindowConfig(options);

    // 如果未指定位置，则居中显示
    const x =
      options.x !== undefined
        ? options.x
        : Math.floor((screenWidth - windowWidth) / 2);
    const y =
      options.y !== undefined
        ? options.y
        : Math.floor((screenHeight - windowHeight) / 2);

    // 创建一个新的 Electron 窗口来承载嵌入的窗口
    const embeddedWindow = new BrowserWindow({
      width: windowWidth,
      height: windowHeight,
      x: x,
      y: y,
      title: `嵌入窗口 - ${options.exePath || "Unknown"}`,
      show: false,
      backgroundColor: "#ffffff", // 白色背景确保可见
      transparent: false,
      frame: false, // 无边框，避免干扰
      useContentSize: true,
      webPreferences: {
        nodeIntegration: false,
        sandbox: false,
        offscreen: false,
        backgroundThrottling: false,
        nodeIntegration: false, // 关闭node集成以减少干扰
        webSecurity: false,
        allowRunningInsecureContent: true,
        contextIsolation: false,
        webviewTag: false, // 禁用webview避免冲突
        spellcheck: false,
        disableHtmlFullscreenWindowResize: true,
        hardwareAcceleration: false,
      },
      // 关键：设置窗口属性以确保正确交互
      resizable: false, // 避免调整大小干扰嵌入窗口
      skipTaskbar: false, // 显示在任务栏
      focusable: true, // 确保窗口可以获得焦点
      alwaysOnTop: false, // 不要置顶，避免层级问题
      acceptFirstMouse: true, // 接受首次鼠标点击
    });

    // 加载空白页面（已更新，允许交互）
    embeddedWindow.loadFile(path.join(__dirname, "blank.html"));

    // 等待窗口完全加载
    await new Promise((resolve) => {
      embeddedWindow.webContents.once("did-finish-load", () => {
        setTimeout(resolve, 500); // 稍短的延迟，确保句柄可用
      });
    });

    const contentAreaWidth = windowWidth - menuSidebarWidth; // 内容区域宽度 = 窗口宽度 - 菜单宽度
    const contentAreaHeight = windowHeight - titleBarHeight; // 内容区域高度 = 窗口高度 - 标题栏高度

    // 嵌入窗口位置：从内容区域的左上角开始（与 .content-area 的布局一致）
    // .content-area 没有 padding，所以从 (0, titleBarHeight) 开始

    // 获取新窗口的原生句柄并创建嵌入窗口
    const windowId = nativeAddon.createEmbeddedWindow(
      embeddedWindow.getNativeWindowHandle(),
      {
        exePath: options.exePath,
        args: options.args || "",
        x: embeddedX,
        y: embeddedY,
        width: contentAreaWidth, // 使用内容区域宽度
        height: contentAreaHeight, // 使用内容区域高度
      }
    );
    console.log("[windowId]:", windowId);

    // 保存窗口映射
    embeddedWindows.set(windowId, embeddedWindow);

    // 窗口关闭时清理
    embeddedWindow.on("closed", () => {
      if (nativeAddon) {
        try {
          nativeAddon.destroyWindow(windowId);
        } catch (error) {
          console.error("Error destroying window on close:", error);
        }
      }
      embeddedWindows.delete(windowId);
    });

    // 关键：设置窗口层级和焦点
    setTimeout(() => {
      embeddedWindow.show();
      // 确保窗口接收输入焦点
      embeddedWindow.focus();
      // 确保窗口在正确的层级
      embeddedWindow.moveTop();
    }, 300);

    console.log("Created embedded window:", windowId);
    return { success: true, windowId };
  } catch (error) {
    console.error("Failed to create embedded window:", error);
    return { success: false, error: error.message };
  }
});

// 更新窗口
ipcMain.handle("update-window", async (event, windowId, options) => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    // 同时更新对应的 BrowserWindow，获取当前窗口尺寸
    const embeddedWindow = embeddedWindows.get(windowId);
    if (!embeddedWindow || embeddedWindow.isDestroyed()) {
      throw new Error("Window not found");
    }

    // 获取当前窗口尺寸
    const [currentWidth, currentHeight] = embeddedWindow.getContentSize();
    // const windowWidth = options.width !== undefined ? options.width : currentWidth;
    // const windowHeight = options.height !== undefined ? options.height : currentHeight;
    const {
      windowWidth,
      windowHeight,
      titleBarHeight,
      menuSidebarWidth,
      embeddedX,
      embeddedY,
    } = UseWindowConfig(options);

    // 计算嵌入窗口在内容区域中的位置（与创建时保持一致）
    // 参考 blank.html 的 CSS 布局：
    // - .title-bar: height: 40px
    // - .menu-sidebar: width: 200px
    // - .content-area: flex: 1, 无 padding/margin
    // const titleBarHeight = 40; // 对应 .title-bar 的高度
    // const menuSidebarWidth = 200; // 对应 .menu-sidebar 的宽度
    const contentAreaWidth = windowWidth - menuSidebarWidth; // 内容区域宽度
    const contentAreaHeight = windowHeight - titleBarHeight; // 内容区域高度
    // 更新原生窗口
    const result = nativeAddon.updateWindow(windowId, {
      x: embeddedX,
      y: embeddedY,
      width: contentAreaWidth,
      height: contentAreaHeight,
    });

    // 更新 BrowserWindow 尺寸
    if (options.width !== undefined || options.height !== undefined) {
      embeddedWindow.setContentSize(windowWidth, windowHeight);
    }

    // 如果指定了新位置，更新窗口位置（并保持居中）
    if (options.x !== undefined || options.y !== undefined) {
      const { screen } = require("electron");
      const primaryDisplay = screen.getPrimaryDisplay();
      const { width: screenWidth, height: screenHeight } =
        primaryDisplay.workAreaSize;

      const x =
        options.x !== undefined
          ? options.x
          : Math.floor((screenWidth - windowWidth) / 2);
      const y =
        options.y !== undefined
          ? options.y
          : Math.floor((screenHeight - windowHeight) / 2);

      embeddedWindow.setPosition(x, y);
    }

    // 重要：调整后重新聚焦
    embeddedWindow.focus();

    console.log("Updated window:", windowId, result);
    return { success: result };
  } catch (error) {
    console.error("Failed to update window:", error);
    return { success: false, error: error.message };
  }
});

// 显示/隐藏窗口
ipcMain.handle("show-window", async (event, windowId, show) => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    // 更新原生窗口
    const result = nativeAddon.showWindow(windowId, show);

    // 同时显示/隐藏对应的 BrowserWindow
    const embeddedWindow = embeddedWindows.get(windowId);
    if (embeddedWindow && !embeddedWindow.isDestroyed()) {
      if (show) {
        embeddedWindow.show();
        embeddedWindow.focus(); // 关键：确保获得焦点
      } else {
        embeddedWindow.hide();
      }
    }

    console.log("Show/Hide window:", windowId, show, result);
    return { success: result };
  } catch (error) {
    console.error("Failed to show/hide window:", error);
    return { success: false, error: error.message };
  }
});

// 销毁窗口
ipcMain.handle("destroy-window", async (event, windowId) => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    // 先关闭对应的 BrowserWindow
    const embeddedWindow = embeddedWindows.get(windowId);
    if (embeddedWindow && !embeddedWindow.isDestroyed()) {
      embeddedWindow.close();
    }

    const result = nativeAddon.destroyWindow(windowId);
    embeddedWindows.delete(windowId);
    console.log("Destroyed window:", windowId, result);
    return { success: result };
  } catch (error) {
    console.error("Failed to destroy window:", error);
    return { success: false, error: error.message };
  }
});

// 获取所有窗口ID
ipcMain.handle("get-all-window-ids", async () => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    const ids = nativeAddon.getAllWindowIds();
    return { success: true, windowIds: ids };
  } catch (error) {
    console.error("Failed to get window IDs:", error);
    return { success: false, error: error.message };
  }
});

// 清理所有窗口
ipcMain.handle("cleanup-all", async () => {
  if (!nativeAddon) {
    throw new Error("Native addon not loaded");
  }

  try {
    nativeAddon.cleanupAll();
    console.log("Cleaned up all windows");
    return { success: true };
  } catch (error) {
    console.error("Failed to cleanup:", error);
    return { success: false, error: error.message };
  }
});

app.whenReady().then(() => {
  createWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("before-quit", () => {
  // 关闭所有嵌入窗口
  embeddedWindows.forEach((window) => {
    if (!window.isDestroyed()) {
      window.close();
    }
  });
  embeddedWindows.clear();

  // 确保在退出前清理所有窗口
  if (nativeAddon) {
    try {
      nativeAddon.cleanupAll();
    } catch (error) {
      console.error("Error during cleanup:", error);
    }
  }
});
