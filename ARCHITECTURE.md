# StreamBox playback architecture

## Runtime path

1. `MainWindow` owns presentation and user interaction only.
2. `PlayerEngine` owns the public state machine and Qt audio output.
3. `DecoderWorker` runs FFmpeg open, network read, demux, decode, filtering and frame conversion outside the GUI thread.
4. Audio passes through `abuffer -> atempo -> aformat -> abuffersink`, producing S16 stereo 48 kHz PCM for `QAudioSink`.
5. Audio PTS is the master clock. Video frames more than 80 ms late are dropped; early frames are delayed in bounded slices.

The current decoder uses a deliberately bounded streaming pipeline: one demux packet and one decoded frame are owned by the worker at a time. Audio production is paced by the filtered sample duration, preventing the queued Qt signal stream from growing without limit.

## Cancellation and seeking

- Stop, media replacement and application shutdown set the FFmpeg interrupt flag.
- Network retry waits are split into 50 ms slices so cancellation remains responsive.
- Seek flushes both codec buffers, rebuilds the audio filter, resets the audio clock and invalidates pre-seek output.
- Unknown-duration or non-seekable inputs disable the UI progress control and are presented as live media.

## Network policy

- Open/read timeout: 10 seconds.
- FFmpeg reconnect is enabled for streamed inputs and transient network failures.
- Initial connection retries: three retries with 1, 2 and 4 second backoff.
- User cancellation interrupts both blocking FFmpeg I/O and pending backoff.
