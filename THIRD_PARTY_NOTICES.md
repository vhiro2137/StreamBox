# Third-party notices

StreamBox links dynamically to Qt and FFmpeg. Their binaries and SDKs are not committed to this repository.

## FFmpeg

- Project: https://ffmpeg.org/
- Source: https://github.com/FFmpeg/FFmpeg
- Version used for local validation: 8.1.2
- License: LGPL 2.1 or later by default; optional build components can change the effective license. The StreamBox build script does not enable GPL or non-free components.

The bounded playback policy, clock thresholds and interruptible I/O design were informed by FFmpeg's `ffplay` reference implementation. StreamBox uses a Qt/C++ implementation and does not embed SDL or copy ffplay as a source file.

## QtAVPlayer reference

- Project: https://github.com/valbok/QtAVPlayer
- License: MIT
- Usage: architectural reference for Qt/FFmpeg frame delivery, seeking tests and bounded buffering. The library is not vendored or linked into StreamBox.

## Qt

- Project: https://www.qt.io/
- Modules: Qt Core, GUI, Widgets, Multimedia and Test
- Deployment must comply with the license selected for the installed Qt distribution.
