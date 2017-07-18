Based on VirtualAudioRenderer, on SaneAr, and on the DirectShow SDK Scope and Metronome samples.

Can playback 8-bit, 16-bit, 32-bit and float content, depending on the capabilities of the OpenAL driver.
Supports mono, stereo, quad, 5.1 and 7.1.
Works with audio.
Works with video, but loses lipsync on seek.

TODO:
- Remove invalid comments
- Fix loss of audio sync on seek
- Fix crash on reload
- Add clock correction:

  -- Correct clock sent to all filters by changing its reference clock, or
  
  -- Sync sound card clock with system clock by resampling with AL_PITCH
  
- Add UI with total latency, number of buffers, device selection and statistics display
