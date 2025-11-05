const { ipcRenderer } = require("electron");

// 存储当前打开的窗口
const openWindows = new Map();

// 日志函数
function log(message, type = "info") {
  const logDiv = document.getElementById("log");
  const timestamp = new Date().toLocaleTimeString();
  const color =
    type === "error" ? "red" : type === "success" ? "green" : "black";
  logDiv.innerHTML += `<div style="color: ${color}">[${timestamp}] ${message}</div>`;
  logDiv.scrollTop = logDiv.scrollHeight;
}

// 更新窗口列表
async function updateWindowList() {
  try {
    const result = await ipcRenderer.invoke("get-all-window-ids");
    const listDiv = document.getElementById("windowList");

    if (result.success && result.windowIds.length > 0) {
      listDiv.innerHTML = "<h3>已打开的窗口：</h3>";
      result.windowIds.forEach((id) => {
        const info = openWindows.get(id);
        const div = document.createElement("div");
        div.className = "window-item";
        div.innerHTML = `
                    <strong>${id}</strong><br>
                    程序: ${info ? info.exePath : "Unknown"}<br>
                    <button onclick="focusWindow('${id}')">聚焦</button>
                    <button onclick="hideWindow('${id}')">隐藏</button>
                    <button onclick="showWindow('${id}')">显示</button>
                    <button onclick="moveWindow('${id}')">移动</button>
                    <button onclick="destroyWindow('${id}')">销毁</button>
                `;
        listDiv.appendChild(div);
      });
    } else {
      listDiv.innerHTML = "<p>没有打开的窗口</p>";
    }
  } catch (error) {
    log("更新窗口列表失败: " + error.message, "error");
  }
}

// 聚焦窗口（新增功能）
window.focusWindow = async (windowId) => {
  try {
    log(`正在聚焦窗口: ${windowId}`);
    // 通过更新窗口来触发聚焦
    const result = await ipcRenderer.invoke("update-window", windowId, {
      width: 800, // 保持当前尺寸
      height: 600,
    });

    if (result.success) {
      log(`窗口已聚焦: ${windowId}`, "success");
    } else {
      log(`聚焦窗口失败: ${windowId}`, "error");
    }
  } catch (error) {
    log(`聚焦窗口异常: ${error.message}`, "error");
  }
};

// 创建窗口
window.createWindow = async () => {
  const exePath = document.getElementById("exePath").value;
  const args = document.getElementById("args").value;
  const x = parseInt(document.getElementById("x").value) || 0;
  const y = parseInt(document.getElementById("y").value) || 0;
  const width = 1200;
  const height = 700;

  if (!exePath) {
    log("请输入程序路径", "error");
    return;
  }

  try {
    log("正在创建窗口...");
    log(`参数信息width: ${width}, height: ${height}`);
    const result = await ipcRenderer.invoke("create-embedded-window", {
      exePath,
      args,
      x,
      y,
      width,
      height,
    });

    if (result.success) {
      openWindows.set(result.windowId, { exePath, args });
      log(`窗口创建成功: ${result.windowId}`, "success");
      await updateWindowList();
      // 自动聚焦新创建的窗口
      await new Promise((resolve) => {
        setTimeout(resolve, 100);
      });
      focusWindow(result.windowId);
      await new Promise((resolve) => {
        hideWindow(result.windowId);
        setTimeout(resolve, 50);
      });

      showWindow(result.windowId);
    } else {
      log(`创建窗口失败: ${result.error}`, "error");
    }
  } catch (error) {
    log(`创建窗口异常: ${error.message}`, "error");
  }
};

// 销毁窗口
window.destroyWindow = async (windowId) => {
  try {
    log(`正在销毁窗口: ${windowId}`);
    const result = await ipcRenderer.invoke("destroy-window", windowId);

    if (result.success) {
      openWindows.delete(windowId);
      log(`窗口销毁成功: ${windowId}`, "success");
      await updateWindowList();
    } else {
      log(`销毁窗口失败: ${windowId}`, "error");
    }
  } catch (error) {
    log(`销毁窗口异常: ${error.message}`, "error");
  }
};

// 隐藏窗口
window.hideWindow = async (windowId) => {
  try {
    const result = await ipcRenderer.invoke("show-window", windowId, false);
    if (result.success) {
      log(`窗口已隐藏: ${windowId}`, "success");
    } else {
      log(`隐藏窗口失败: ${windowId}`, "error");
    }
  } catch (error) {
    log(`隐藏窗口异常: ${error.message}`, "error");
  }
};

// 显示窗口
window.showWindow = async (windowId) => {
  try {
    const result = await ipcRenderer.invoke("show-window", windowId, true);
    if (result.success) {
      log(`窗口已显示: ${windowId}`, "success");
      // 显示后自动聚焦
      setTimeout(() => focusWindow(windowId), 200);
    } else {
      log(`显示窗口失败: ${windowId}`, "error");
    }
  } catch (error) {
    log(`显示窗口异常: ${error.message}`, "error");
  }
};

// 移动窗口（示例：移动到随机位置）
window.moveWindow = async (windowId) => {
  try {
    const x = Math.floor(Math.random() * 400);
    const y = Math.floor(Math.random() * 200);
    const result = await ipcRenderer.invoke("update-window", windowId, {
      x,
      y,
      width: 800,
      height: 600,
    });

    if (result.success) {
      log(`窗口已移动到: (${x}, ${y})`, "success");
    } else {
      log(`移动窗口失败: ${windowId}`, "error");
    }
  } catch (error) {
    log(`移动窗口异常: ${error.message}`, "error");
  }
};

// 清理所有窗口
window.cleanupAll = async () => {
  try {
    log("正在清理所有窗口...");
    const result = await ipcRenderer.invoke("cleanup-all");

    if (result.success) {
      openWindows.clear();
      log("所有窗口已清理", "success");
      await updateWindowList();
    } else {
      log("清理窗口失败", "error");
    }
  } catch (error) {
    log(`清理窗口异常: ${error.message}`, "error");
  }
};

// 预设程序快捷按钮
window.createNotepad = () => {
  document.getElementById("exePath").value =
    "C:\\Windows\\System32\\notepad.exe";
  document.getElementById("args").value = "";
};

window.createCalc = () => {
  document.getElementById("exePath").value =
    "C:\\Program Files\\Typora\\Typora.exe";
  document.getElementById("args").value = "";
};

window.createMspaint = () => {
  document.getElementById("exePath").value =
    "C:\\Windows\\System32\\mspaint.exe";
  document.getElementById("args").value = "";
};

// 页面加载完成后初始化
document.addEventListener("DOMContentLoaded", () => {
  log("BrowserWindowTool 已就绪", "success");
  updateWindowList();
});
