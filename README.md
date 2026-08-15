# StreamBox UI

StreamBox 是依据 `PRD_OnlineMediaPlayer.md`、`DESIGN_OnlineMediaPlayer.md` 与 HTML 原型实现的原生 Qt Widgets 桌面播放器界面。

![StreamBox 界面预览](streambox-ui-preview-windows.png)

## 技术栈

- Qt 6.11 Widgets
- C++17
- CMake 3.21+
- MinGW 13.1 x64（本机已验证）

项目没有使用 Qt WebEngine，也没有将 HTML 原型嵌入应用。HTML 仅作为视觉与交互参考。

FFmpeg 8.1.2 已使用与 Qt Kit 相同的 MinGW 13.1 x64 编译到 `third_party\ffmpeg-sdk` 并接入应用。目前支持真实本地及 HTTP 媒体解封装、音视频解码、Qt 音频输出、视频渲染、暂停、停止、进度与 Seek。完整环境结果参见 `TECH_STACK_AUDIT.md`。

## 本地依赖（不纳入仓库）

仓库不会提交 Qt、MSYS2、FFmpeg 源码、编译产物或 SDK。构建前请准备：

- Qt 6.5+（Widgets、Multimedia）与对应 MinGW 64-bit Kit
- CMake 3.21+ 与 Ninja
- MSYS2（默认放置于 `.tooling\msys64`）
- FFmpeg 8.1.2 源码（本机默认路径为 `D:\ffmpeg-8.1.2\ffmpeg-8.1.2`）

首次构建前生成本地 FFmpeg SDK：

```powershell
$env:STREAMBOX_FFMPEG_SOURCE='D:\ffmpeg-8.1.2\ffmpeg-8.1.2'
& '.\scripts\build_ffmpeg_mingw.ps1'
```

如 Qt MinGW 不在默认的 `/d/Qt/Tools/mingw1310_64/bin`，可在 MSYS2 中通过 `STREAMBOX_QT_MINGW` 指定。生成的 `third_party\ffmpeg-sdk` 仅供本地编译使用，已被 `.gitignore` 排除。

## 使用 Qt Creator 打开

1. 启动 `D:\Qt\Tools\QtCreator\bin\qtcreator.exe`。
2. 选择“打开项目”，打开本目录下的 `CMakeLists.txt`。
3. 选择 `Desktop Qt 6.11.0 MinGW 64-bit` Kit。
4. 配置项目后构建并运行 `StreamBox`。

## 命令行构建

```powershell
& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH='D:\Qt\6.11.0\mingw_64' `
  -DCMAKE_CXX_COMPILER='D:\Qt\Tools\mingw1310_64\bin\g++.exe' `
  -DCMAKE_MAKE_PROGRAM='D:\Qt\Tools\Ninja\ninja.exe'

& 'D:\Qt\Tools\CMake_64\bin\cmake.exe' --build build --parallel
```

## 已实现的界面与交互

- 深色主窗口、视频画布、播放列表和三段式底部控制区
- 空闲、播放、暂停、停止、音频、连接、缓冲、错误和播放结束状态
- 文件选择与文件拖放
- HTTP/HTTPS URL 输入及格式校验
- 播放/暂停、停止、上下首、进度、音量和静音
- 0.5×～2.0× 倍速菜单
- 顺序播放、列表循环和单曲循环
- 播放列表显示/隐藏、添加、删除和清空
- 视频全屏及 Esc/双击退出
- PRD 中定义的键盘快捷键
- 960×600 最小窗口和响应式布局

## 播放器内核进度

- 已实现：FFmpeg 8.1.2 动态链接、本地/HTTP 输入、常见音视频解码、S16 48kHz 双声道重采样、Qt `QAudioSink`、BGRA 视频帧转换、真实进度、暂停、停止、Seek、中断回调和基础重连。
- 已验证：Qt 官方 H.264/AAC MP4、本地 WAV 和 localhost HTTP MP4。
- 后续质量项：基于 `atempo` 的音频保调倍速、更精确的长期音画同步、网络缓冲水位、三次退避重试和长时压力测试。

## Windows 部署

```powershell
& '.\scripts\deploy_windows.ps1'
```

输出目录为 `dist\StreamBox`。

## 自动化界面截图

程序提供开发验收参数，不影响正常运行：

```powershell
$env:STREAMBOX_SCREENSHOT='D:\Qt_project\codex_project\streambox-ui-preview.png'
& '.\build\StreamBox.exe'
```

应用显示后会自动保存截图并退出。
