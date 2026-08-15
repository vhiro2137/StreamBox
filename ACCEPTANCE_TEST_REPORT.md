# StreamBox V1 acceptance report

Updated: 2026-08-16

| Case | Status | Evidence |
|---|---|---|
| AC-01 local audio | Passed | WAV, MP3, FLAC, Vorbis and Opus produced PCM. Qt selected the physical `耳机 (Realtek(R) Audio)` endpoint and processed a 3.00-second audible tone with sink error 0; audible output was confirmed by the user. |
| AC-02 local video | Passed | MP4, MKV, MOV, WebM and AVI produced decoded frames; H.264/AAC GUI rendering was visually verified. |
| AC-03 HTTP/HTTPS | Partial | In-process HTTP playback passed. Public HTTPS connection/cancellation passed; repeatable HTTPS throughput depends on the current network. |
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
| AC-14 two-hour playback | Partial | The application remained alive for 120 minutes and exited normally (240 samples); working set stabilized near 71–74 MB and threads near 22–25. The current harness does not prove that media output continued for the full duration, and handles rose from about 935 after warm-up to 1005 at the end, so continuous-playback and handle-growth investigation remain open. |
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
