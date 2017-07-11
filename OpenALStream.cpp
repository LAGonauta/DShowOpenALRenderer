// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32

#include <windows.h>
#include <climits>
#include <cstring>
#include <thread>
#include <array>

#include "OpenALStream.h"

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
bool OpenALStream::Start()
{
  if (!alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
  {
    printf_s("OpenAL: can't find sound devices");
    return false;
  }

  const char* default_device_dame = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
  printf_s("Found OpenAL device %s", default_device_dame);

  ALCdevice* device = alcOpenDevice(default_device_dame);
  if (!device)
  {
    printf_s("OpenAL: can't open device %s", default_device_dame);
    return false;
  }

  ALCcontext* context = alcCreateContext(device, nullptr);
  if (!context)
  {
    alcCloseDevice(device);
    printf_s("OpenAL: can't create context for device %s", default_device_dame);
    return false;
  }

  alcMakeContextCurrent(context);
  //m_run_thread.Set();
  m_thread = std::thread(&OpenALStream::SoundLoop, this);
  return true;
}

void OpenALStream::Stop()
{
  //m_run_thread.Clear();
  // kick the thread if it's waiting
  //m_sound_sync_event.Set();

  m_thread.join();

  alSourceStop(m_source);
  alSourcei(m_source, AL_BUFFER, 0);

  // Clean up buffers and sources
  alDeleteSources(1, &m_source);
  m_source = 0;
  alDeleteBuffers(OAL_BUFFERS, m_buffers.data());

  ALCcontext* context = alcGetCurrentContext();
  ALCdevice* device = alcGetContextsDevice(context);

  alcMakeContextCurrent(nullptr);
  alcDestroyContext(context);
  alcCloseDevice(device);
}

void OpenALStream::SetVolume(int volume)
{
  m_volume = (float)volume / 100.0f;

  if (m_source)
    alSourcef(m_source, AL_GAIN, m_volume);
}

void OpenALStream::Update()
{
  //m_sound_sync_event.Set();
}

void OpenALStream::Clear(bool mute)
{
  m_muted = mute;

  if (m_muted)
  {
    alSourceStop(m_source);
  }
  else
  {
    alSourcePlay(m_source);
  }
}

static ALenum CheckALError(const char* desc)
{
  ALenum err = alGetError();

  if (err != AL_NO_ERROR)
  {
    std::string type;

    switch (err)
    {
    case AL_INVALID_NAME:
      type = "AL_INVALID_NAME";
      break;
    case AL_INVALID_ENUM:
      type = "AL_INVALID_ENUM";
      break;
    case AL_INVALID_VALUE:
      type = "AL_INVALID_VALUE";
      break;
    case AL_INVALID_OPERATION:
      type = "AL_INVALID_OPERATION";
      break;
    case AL_OUT_OF_MEMORY:
      type = "AL_OUT_OF_MEMORY";
      break;
    default:
      type = "UNKNOWN_ERROR";
      break;
    }

    printf_s("Error %s: %08x %s", desc, err, type.c_str());
  }

  return err;
}

static bool IsCreativeXFi()
{
  return strstr(alGetString(AL_RENDERER), "X-Fi") != nullptr;
}

void OpenALStream::SoundLoop()
{
  //Common::SetCurrentThreadName("Audio thread - openal");

  bool float32_capable = alIsExtensionPresent("AL_EXT_float32") != 0;
  bool surround_capable = alIsExtensionPresent("AL_EXT_MCFORMATS") || IsCreativeXFi();

  // As there is no extension to check for 32-bit fixed point support
  // and we know that only a X-Fi with hardware OpenAL supports it,
  // we just check if one is being used.
  bool fixed32_capable = IsCreativeXFi();

  uint32_t frames_per_buffer;
  // Can't have zero samples per buffer
  if (m_latency > 0)
  {
    frames_per_buffer = m_frequency / 1000 * m_latency / OAL_BUFFERS;
  }
  else
  {
    frames_per_buffer = m_frequency / 1000 * 1 / OAL_BUFFERS;
  }

  if (frames_per_buffer > OAL_MAX_FRAMES)
  {
    frames_per_buffer = OAL_MAX_FRAMES;
  }

  printf_s("Using %d buffers, each with %d audio frames for a total of %d.", OAL_BUFFERS,
           frames_per_buffer, frames_per_buffer * OAL_BUFFERS);

  // Should we make these larger just in case the mixer ever sends more samples
  // than what we request?
  m_buffers.resize(num_buffers);
  m_source = 0;

  // Clear error state before querying or else we get false positives.
  ALenum err = alGetError();

  // Generate some AL Buffers for streaming
  alGenBuffers(OAL_BUFFERS, (ALuint*)m_buffers.data());
  err = CheckALError("generating buffers");

  // Generate a Source to playback the Buffers
  alGenSources(1, &m_source);
  err = CheckALError("generating sources");

  // Set the default sound volume as saved in the config file.
  alSourcef(m_source, AL_GAIN, m_volume);

  // TODO: Error handling
  // ALenum err = alGetError();

  unsigned int next_buffer = 0;
  unsigned int num_buffers_queued = 0;
  ALint state = 0;

  while (m_run_thread)
  {
    // Block until we have a free buffer
    int num_buffers_processed = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_buffers_processed);
    if (num_buffers_queued == OAL_BUFFERS && !num_buffers_processed)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    if (true)
    {
      // Remove the Buffer from the Queue.
      if (num_buffers_processed)
      {
        std::array<ALuint, OAL_BUFFERS> unqueued_buffer_ids;
        alSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
        err = CheckALError("unqueuing buffers");

        num_buffers_queued -= num_buffers_processed;
      }

      size_t min_frames = frames_per_buffer;

      size_t available_samples = 0;
      if (m_speaker_layout == Surround6)
      {
        if (m_media_type == bit16)
        {
          available_samples = m_audio_buffer_queue.unsafe_size();
        }

        if (available_samples < min_frames * SURROUND_CHANNELS)
          continue;

        if (available_samples % 6 != 0)
        {
          available_samples = available_samples - available_samples % 6;
        }

        // Get data from queue
        std::array<__int16, OAL_MAX_FRAMES> short_data;

        for (size_t i = 0; i < available_samples; ++i)
        {
          bool trypop = m_audio_buffer_queue.try_pop(short_data[i]);
          if (trypop == false)
          {
            short_data[i] = 0;
          }
        }

        alBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN16, short_data.data(),
          available_samples * SIZE_SHORT, m_frequency);
      }
      else
      {
        if (m_media_type == bit16)
        {
          available_samples = m_audio_buffer_queue.unsafe_size();
        }

        if (available_samples < min_frames * STEREO_CHANNELS * (num_buffers - num_buffers_queued))
          continue;

        if (available_samples % STEREO_CHANNELS != 0)
          --available_samples;

        // samples that will be buffered by OpenAL
        available_samples = available_samples / (num_buffers - num_buffers_queued);

        // Get data from queue
        std::array<__int16, OAL_MAX_FRAMES> short_data;

        for (size_t i = 0; i < available_samples; ++i)
        {
          short value = 0;
          bool trypop = m_audio_buffer_queue.try_pop(value);
          if (trypop)
          {
            short_data[i] = value;
          }
          else
          {
            OutputDebugString(L"Failed to pop! \n");
            short_data[i] = 0;
          }
        }

        alBufferData(m_buffers[next_buffer], AL_FORMAT_STEREO16, short_data.data(),
          available_samples * SIZE_SHORT, m_frequency);
      }
      err = CheckALError("buffering data");

      alSourceQueueBuffers(m_source, 1, &m_buffers[next_buffer]);
      err = CheckALError("queuing buffers");

      num_buffers_queued++;
      next_buffer = (next_buffer + 1) % OAL_BUFFERS;

      alGetSourcei(m_source, AL_SOURCE_STATE, &state);
      if (state != AL_PLAYING)
      {
        // Buffer underrun occurred, resume playback
        alSourcePlay(m_source);
        err = CheckALError("occurred resuming playback");
      }
    }
  }
}

#endif  // _WIN32
