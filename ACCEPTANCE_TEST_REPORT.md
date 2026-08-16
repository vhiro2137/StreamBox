# StreamBox V1 acceptance report

Updated: 2026-08-16

| Case | Status | Evidence |
|---|---|---|
| AC-01 local audio | Passed | WAV, MP3, FLAC, Vorbis and Opus produced PCM. Qt selected the physical `耳机 (Realtek(R) Audio)` endpoint and processed a 3.00-second audible tone with sink error 0; audible output was confirmed by the user. |
| AC-02 local video | Passed | MP4, MKV, MOV, WebM and AVI produced decoded frames. After the PCM buffering and playback-clock fix, the user confirmed H.264/AAC playback was smooth with no audible noise or visible stutter. |
| AC-03 HTTP/HTTPS | Passed | In-process HTTP playback passed. A public W3C HTTPS MP4 completed open, demux and decoded-output probing with exit code 0; failed-connect cancellation is covered separately. |
| AC-04 invalid network | Passed | Failed connection enters application retry policy and Stop cancels pending retry. |
| AC-05 pause/resume | Passed | Integration test verifies output pauses and resumes from the same worker. |
| AC-06 seek | Passed | Automated 3000 ms audio seek and visual 20000 ms video seek resumed without pre-target output. |
| AC-07 playback rates | Passed | FFmpeg `atempo` tested at 2.0×; GUI audio/video verified at 1.5×. |
| AC-08 previous/next | Passed | Mode boundaries and the three-second previous rule are unit tested and used by the UI. |
| AC-09 natural completion | Passed | Completion index policy and worker end signal are tested. |
| AC-10 resize/fullscreen | Passed | Aspect-preserving render and three-second control hiding were visually verified. |
| AC-11 audio-only UI | Passed | WAV playback displays the audio canvas and disables video fullscreen. |
| AC-12 live/non-seekable | Passed | Seekability policy and UI live state are unit tested. |
| AC-13 disconnect/recovery | Partial | HTTP premature close and failed-connect cancellation are tested; physical adapter disconnect remains manual. |
| AC-14 two-hour playback | Passed | The post-fix 120-minute AVI audio/video loop completed normally with 240 samples and an empty error log. After warm-up, working set stayed within 53.02–92.97 MB, threads within 6–29, and handles changed from 945 to 946 (range 910–950). The original 70-handle growth was eliminated. |
| AC-15 100 switches | Passed | One hundred worker replacements completed without errors or a remaining thread. |

## Format matrix

Official FFmpeg, Xiph and W3C samples are stored under ignored `.tooling`. The probe reports success only after opening and producing non-empty decoded output.

| Format | Result |
|---|---|
| PCM/WAV | Passed |
| MP3 | Passed |
| FLAC | Passed |
| Vorbis/OGG | Passed |
| Opus/OGG | Passed |
| AAC/MP4 | Passed |
| H.264/AAC MP4 | Passed |
| MKV | Passed |
| MOV | Passed |
| WebM | Passed |
| AVI | Passed |

HEVC, VP9, AV1 and MPEG-TS remain capability-matrix extensions, not V1 core blockers. HLS, RTSP and RTMP are V1.1 scope in the PRD.

## 2026-08-16 playback-quality regression

- Preserved partially written PCM instead of dropping the unwritten tail returned by `QIODevice::write`.
- Added a 5 ms audio drain loop based on `QAudioSink::bytesFree`.
- Replaced decoded-audio-PTS pacing with a continuous playback clock and a bounded 250 ms decode lead.
- Delayed natural completion until the physical audio sink reaches `IdleState`, preventing truncated tails.
- Reused the idle audio sink between natural playlist loops to stop Windows handle accumulation.
- PCM regression verifies frame alignment, stereo sample integrity and expected 2.0× output size.
- `danking.mp4` and `微灵医疗一面.mp4` were identified as H.264 1920×1080 + AAC. Both produced audio and video frames, rendered visible GUI screenshots, froze at 2000 ms for a 700 ms pause interval, and resumed to 2500 ms with new video frames.
- Queued video/position signals are ignored while paused, preventing the final pre-pause event from advancing the displayed frame or progress.
- The first decoded video frame is always displayed before late-frame policy begins, preventing initial clock skew from causing persistent audio-only playback.
