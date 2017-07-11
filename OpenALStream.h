// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include <include/OpenAL/al.h>
#include <include/OpenAL/alc.h>

// OpenAL requires a minimum of two buffers, three or more recommended
#define OAL_BUFFERS 3
#define OAL_MAX_FRAMES 65536
#define STEREO_CHANNELS 2
#define SURROUND_CHANNELS 6  // number of channels in surround mode
#define SIZE_SHORT 2
#define SIZE_INT32 4
#define SIZE_FLOAT 4  // size of a float in bytes
#define FRAME_STEREO_SHORT STEREO_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_FLOAT SURROUND_CHANNELS* SIZE_FLOAT
#define FRAME_SURROUND_SHORT SURROUND_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_INT32 SURROUND_CHANNELS* SIZE_INT32

// From AL_EXT_float32
#ifndef AL_FORMAT_STEREO_FLOAT32
#define AL_FORMAT_STEREO_FLOAT32 0x10011
#endif

// From AL_EXT_MCFORMATS
#ifndef AL_FORMAT_51CHN16
#define AL_FORMAT_51CHN16 0x120B
#endif
#ifndef AL_FORMAT_51CHN32
#define AL_FORMAT_51CHN32 0x120C
#endif

// Only X-Fi on Windows supports the alext AL_FORMAT_STEREO32 alext for now,
// but it is not documented or in "OpenAL/include/al.h".
#ifndef AL_FORMAT_STEREO32
#define AL_FORMAT_STEREO32 0x1203
#endif

class OpenALStream final
{
public:
  OpenALStream() : m_source(0) {}
  bool Start();
  void SoundLoop();
  void SetVolume(int volume);
  void Stop();
  void Clear(bool mute);
  void Update();

  concurrency::concurrent_queue<__int16> m_audio_buffer_queue;
  concurrency::concurrent_queue<__int32> m_audio_buffer_queue_int32;
  concurrency::concurrent_queue<float> m_audio_buffer_queue_float;

  enum SpeakerLayout
  {
    Mono,
    Stereo,
    Quad,
    Surround6,
    Surround8
  } m_speaker_layout = Stereo;

  enum MediaType
  {
    bit8,
    bit16,
    bit24,
    bit32,
    bitfloat
  } m_media_type = bit16;

private:
  std::thread m_thread;
  //Common::Flag m_run_thread;
  bool m_run_thread = true;

  //Common::Event m_sound_sync_event;

  uint32_t num_buffers = 3;

  std::vector<ALuint> m_buffers;
  ALuint m_source;
  ALfloat m_volume;

  // Get from settings
  uint32_t m_latency = 30;
  uint32_t m_frequency = 48000;
  bool m_muted = false;
};
