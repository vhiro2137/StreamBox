# StreamBox 技术栈审计

审计日期：2026-08-15

## 结论

实现 PRD 中本地/网络音视频播放所需的开发技术栈已经齐全。实施规划批准后，FFmpeg SDK 已链接进 `StreamBox`，并完成第一版本地及 HTTP 音视频播放链路。

## 已验证组件

| 组件 | 版本/位置 | 状态 |
|---|---|---|
| Qt Creator | `D:\Qt\Tools\QtCreator\bin\qtcreator.exe` | 已安装 |
| Qt | 6.11.0 MinGW x64 | 已安装 |
| Qt Widgets | Qt 6.11.0 | 已安装 |
| Qt Multimedia | Qt 6.11.0 | 已安装 |
| Qt OpenGLWidgets | Qt 6.11.0 | 已安装 |
| Qt Concurrent | Qt 6.11.0 | 已安装 |
| Qt Test | Qt 6.11.0 | 已安装 |
| CMake | `D:\Qt\Tools\CMake_64` | 已安装 |
| Ninja | `D:\Qt\Tools\Ninja` | 已安装 |
| MinGW | GCC 13.1.0 x64 | 已安装 |
| windeployqt | Qt 6.11.0 MinGW x64 | 已安装 |
| MSYS2 Bash | `.tooling\msys64` | 已补齐 |
| GNU Make | 4.4.1 | 已补齐 |
| Pkgconf | 3.0.5 | 已补齐 |
| NASM | 2.16.03 | 已补齐 |
| Diffutils | 3.12 | 已补齐 |
| Perl | 5.42.2 | 已包含 |
| FFmpeg SDK | 8.1.2 MinGW x64 | 已编译并验证 |

## FFmpeg SDK

位置：`third_party\ffmpeg-sdk`

```text
third_party/ffmpeg-sdk/
├─ include/  # FFmpeg 公共头文件
├─ lib/      # MinGW .dll.a 导入库、.lib 和 pkg-config 文件
├─ bin/      # 运行时 DLL
└─ share/    # 示例和文档资源
```

已包含库：

- libavformat 62
- libavcodec 62
- libavfilter 11
- libavutil 60
- libswresample 6
- libswscale 9

构建属性：

- 编译器与 Qt Kit 相同：MinGW GCC 13.1 x64。
- LGPL 2.1 或更高版本配置，未启用 GPL 外部库。
- 共享库构建。
- Windows SChannel TLS，支持 HTTP/HTTPS。
- 包含 H.264、H.265/HEVC、VP9、AV1、AAC、MP3、FLAC、Opus 等内置解码器。
- 包含 MP4、MKV、MOV、WebM、AVI、MPEG-TS、HLS 等解封装器。
- 包含 `atempo`、重采样和图像缩放能力。
- 检测到 D3D11VA 和 DXVA2 硬件加速接口，但首版仍按软件解码规划实施。

## 验证结果

使用与 Qt 相同的 MinGW 编译并运行了 `scripts/verify_ffmpeg_sdk.c`，成功动态加载全部六个 FFmpeg DLL：

```text
FFmpeg: 8.1.2
libavformat: 4066406
libavcodec: 4070502
libavfilter: 724582
libswresample: 394086
libswscale: 591206
ExitCode=0
```

## 可复现构建

- PowerShell 入口：`scripts\build_ffmpeg_mingw.ps1`
- MSYS2 构建脚本：`scripts/build_ffmpeg_mingw.sh`
- 链接验证源码：`scripts/verify_ffmpeg_sdk.c`

## 后续实现项

- 已完成：基于 FFmpeg `atempo` 的 0.5×～2.0× 保调倍速链。
- 已完成：音频 PTS 主时钟、视频 80 ms 迟到丢帧与提前帧延迟策略。
- 已完成：网络超时、FFmpeg 自动重连和 1/2/4 秒三次可中断打开重试。
- 已完成：精确 Seek 的目标前帧丢弃、直播/不可 Seek 判断、全屏控制栏自动隐藏。
- 已验证：本地 WAV、W3C H.264/AAC MP4、1.5× 音视频、2.0× 音频、20 秒精确 Seek、网络失败取消和 Windows 部署目录启动。
- 发布门禁：仍需在目标发布机器上执行 AC-01～AC-14 完整格式矩阵与 2 小时连续播放测试。
