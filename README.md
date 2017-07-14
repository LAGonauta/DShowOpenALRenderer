Based on VirtualAudioRenderer, on the DirectShow SDK Scope and Metronome samples.

Can playback 16-bit stereo and 5.1 surround content.

TODO:
- Remove invalid comments
- Add support for 8-bit, 24-bit, 32-bit and 32-bit float formats
- Add support for Mono, Quad and 7.1 setups
- Add full support for dynamic format change
- Add volume control
- Add clock correction:
  -- Correct clock sent to all filters by changing its reference clock, or
  -- Sync sound card clock with system clock by resampling with AL_PITCH
- Add UI with total latency, number of buffers, device selection and statistics display
