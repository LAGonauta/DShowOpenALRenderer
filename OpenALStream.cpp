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
#include "VirtualAudioRenderer.h"

COpenALStream::~COpenALStream(void)
{
  CloseDevice();
}

COpenALStream::COpenALStream(CScopeInputPin* inputPin, LPUNKNOWN pUnk, HRESULT * phr)
  : CBaseReferenceClock(NAME("OpenAL Stream Clock"), pUnk, phr)
  , m_pCurrentRefClock(0), m_pPrevRefClock(0)
{
  EXECUTE_ASSERT(SUCCEEDED(OpenDevice()));

  m_pinput_pin = inputPin;

  // last time we reported
  m_dwLastMet = 0;

  // similar to m_LastMet, but used to help switch between clocks somehow
  m_dwPrevSystemTime = timeGetTime();

  // what timeGetTime said last time we heard a tick
  m_LastTickTGT = m_dwPrevSystemTime;

  // the number we reported last time we heard a tick
  m_LastTickTime = m_LastTickTGT;

  // the last time we reported (in 100ns units)
  m_rtPrivateTime = (UNITS / MILLISECONDS) * m_dwPrevSystemTime;

  // what timeGetTime said the last time we were asked what time it was
  m_dwLastTGT = m_dwPrevSystemTime;

  // We start off assuming the clock is running at normal speed
  m_msPerTick = m_latency / num_buffers;

  DbgLog((LOG_TRACE, 1, TEXT("Creating clock at ref tgt=%d"), m_LastTickTime));
}

//REFERENCE_TIME COpenALStream::GetPrivateTime()
//{
//  CAutoLock cObjectLock(this);
//
//  /* If the clock has wrapped then the current time will be less than
//  * the last time we were notified so add on the extra milliseconds
//  *
//  * The time period is long enough so that the likelihood of
//  * successive calls spanning the clock cycle is not considered.
//  */
//
//  // This returns the current time in ms according to our special clock.  If
//  // we used timeGetTime() here, our clock would run normally.
//  //DWORD dwTime = MetGetTime();
//  DWORD dwTime = timeGetTime();
//  {
//    REFERENCE_TIME delta = REFERENCE_TIME(dwTime) - REFERENCE_TIME(m_dwPrevSystemTime);
//    if (dwTime < m_dwPrevSystemTime)
//      delta += REFERENCE_TIME(UINT_MAX) + 1;
//
//    m_dwPrevSystemTime = dwTime;
//
//    delta *= (UNITS / MILLISECONDS);
//    m_rtPrivateTime += delta;
//  }
//
//  return m_rtPrivateTime;
//}

void COpenALStream::SetSyncSource(IReferenceClock * pClock)
{
  m_pPrevRefClock = m_pCurrentRefClock;

  if (pClock)
  {
    m_dwPrevSystemTime = timeGetTime();

    if (IsEqualObject(pClock, pUnk()))
    {
      // Sync this clock up to the old one - just to be nice - for now
      m_LastTickTGT = m_dwPrevSystemTime;
      m_LastTickTime = m_LastTickTGT;
      m_rtPrivateTime = (UNITS / MILLISECONDS) * m_dwPrevSystemTime;

      if (m_pPrevRefClock)
      {
        if (SUCCEEDED(m_pPrevRefClock->GetTime(&m_rtPrivateTime)))
        {
          m_dwPrevSystemTime += timeGetTime();
          m_dwPrevSystemTime /= 2;
        }
        else
          ASSERT(FALSE);
      }

      DbgLog((LOG_TRACE, 1, TEXT("*** USING OUR CLOCK : reference is %d at tgt %d"),
        (DWORD)(MILLISECONDS * m_rtPrivateTime / UNITS), m_LastTickTime));

    }
    else
    {
      // Sync our clock up to the new one
      m_LastTickTGT = m_dwPrevSystemTime;
      m_LastTickTime = m_LastTickTGT;
      EXECUTE_ASSERT(SUCCEEDED(pClock->GetTime(&m_rtPrivateTime)));

      m_dwPrevSystemTime += timeGetTime();
      m_dwPrevSystemTime /= 2;

      DbgLog((LOG_TRACE, 1, TEXT("*** USING SOMEONE ELSE'S CLOCK : reference is %d at tgt %d"),
        (DWORD)(MILLISECONDS * m_rtPrivateTime / UNITS), m_LastTickTime));
    }
  }

  m_pCurrentRefClock = pClock;
}

void COpenALStream::ClockController(HDRVR hdrvr, DWORD_PTR dwUser, DWORD_PTR dw2)
{
  COpenALStream* pFilter = (COpenALStream *)dwUser;

  // really need a second worker thread here, because
  // one shouldn't do this much in a wave callback.

  ASSERT(pFilter);

  int spike;

  // look for a spike in this buffer we just recorded

    // don't let anybody else mess with our timing variables
  pFilter->m_csClock.Lock();

  // How long has it been since we saw a tick?
  pFilter->m_SamplesSinceTick += spike;
  DWORD msPerTickPrev = pFilter->m_msPerTick;

  // Even though we just got the callback now, this stuff was
  // recorded who knows how long ago, so what we're doing is not
  // entirely correct... we're assuming that since we just noticed
  // the tick means that it happened right now.  As long as our
  // buffers are really small, and the system is very responsive,
  // this won't be too bad.
  DWORD dwTGT = timeGetTime();

  // deal with clock stopping altogether for a while - pretend
  // it kept ticking at its old rate or else we will think we're
  // way ahead and the clock will freeze for the length of time
  // the clock was stopped
  // So if it's been a while since the last tick, don't use that
  // long interval as a new tempo.  This way you can stop clapping
  // and the movie will keep the current tempo until you start
  // clapping a new tempo.
  // (If it's been > 1.5s since the last tick, this is probably
  //  the start of a new tempo).
  if (pFilter->m_SamplesSinceTick * 1000 / 11025 > 1500)
  {
    DbgLog((LOG_TRACE, 2, TEXT("Ignoring 1st TICK after long gap")));
  }
  else
  {
    // running our clock at the old rate, we'd be here right now
    pFilter->m_LastTickTime = pFilter->m_dwLastMet +
      (dwTGT - pFilter->m_dwLastTGT) *
      625 / pFilter->m_msPerTick;

    pFilter->m_msPerTick = (DWORD)((LONGLONG)
      pFilter->m_SamplesSinceTick * 1000 / 11025);

    pFilter->m_LastTickTGT = dwTGT;

    DbgLog((LOG_TRACE, 2, TEXT("TICK! after %dms, reporting %d tgt=%d"), pFilter->m_msPerTick, pFilter->m_LastTickTime, pFilter->m_LastTickTGT));
  }

  pFilter->m_SamplesSinceTick = 0;
  pFilter->m_csClock.Unlock();

  // we went the whole buffer without seeing a tick.
  //pFilter->m_SamplesSinceTick += len;
}

//
// AyuanX: Spec says OpenAL1.1 is thread safe already
//
STDMETHODIMP COpenALStream::OpenDevice(void)
{
  if (!alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
  {
    printf_s("OpenAL: can't find sound devices");
    return E_FAIL;
  }

  const char* default_device_dame = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);
  printf_s("Found OpenAL device %s", default_device_dame);

  ALCdevice* device = alcOpenDevice(default_device_dame);
  if (!device)
  {
    printf_s("OpenAL: can't open device %s", default_device_dame);
    return E_FAIL;
  }

  ALCcontext* context = alcCreateContext(device, nullptr);
  if (!context)
  {
    alcCloseDevice(device);
    printf_s("OpenAL: can't create context for device %s", default_device_dame);
    return E_FAIL;
  }

  alcMakeContextCurrent(context);
  return S_OK;
}

STDMETHODIMP COpenALStream::CloseDevice(void)
{
  StopDevice();
  Stop();

  return S_OK;
}

STDMETHODIMP COpenALStream::StartDevice(void)
{
  if (m_run_thread == false)
  {
    m_run_thread = true;
    m_thread = std::thread(&COpenALStream::SoundLoop, this);
  }

  return S_OK;
}

STDMETHODIMP COpenALStream::StopDevice(void)
{
  m_run_thread = false;
  if (m_thread.joinable())
  {
    m_thread.join();
  }

  return S_OK;
}

void COpenALStream::Stop()
{
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

DWORD COpenALStream::MetGetTime(void)
{
  // Don't let anybody change our time variables on us while we're using them
  m_csClock.Lock();
  LONGLONG lfudge;

  // how many ms have elapsed since last time we were asked?
  DWORD tgt = timeGetTime();
  LONGLONG lms = tgt - m_LastTickTGT;

  // How many ms do we want to pretend elapsed?
  if (m_msPerTick)
    lfudge = lms * (625) / (LONGLONG)m_msPerTick;
  else
    lfudge = 0; // !!!

                // that's the new time to report
  DWORD dw = m_LastTickTime + (DWORD)lfudge;
  m_csClock.Unlock();

  // Under no circumstances do we let the clock run backwards.  Just stall it.
  if (dw < m_dwLastMet)
  {
    dw = m_dwLastMet;
    DbgLog((LOG_TRACE, 1, TEXT("*** ACK! Tried to go backwards!")));
  }

  DbgLog((LOG_TRACE, 3, TEXT("MetTGT: %dms elapsed. Adjusted to %dms"),
    (int)lms, (int)lfudge));
  DbgLog((LOG_TRACE, 3, TEXT("        returning %d TGT=%d"), (int)dw,
    (int)timeGetTime()));

  m_dwLastMet = dw;
  m_dwLastTGT = tgt;
  return dw;
}

void COpenALStream::SetVolume(int volume)
{
  m_volume = (float)volume / 100.0f;

  if (m_source)
    alSourcef(m_source, AL_GAIN, m_volume);
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

void COpenALStream::SoundLoop()
{
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

      // Get the handle and lock the input pin
      if (!m_pin_locked)
      {
        auto wait_result = WaitForSingleObject(m_pinput_pin->m_input_mutex, INFINITE);
        if (wait_result == WAIT_OBJECT_0)
        {
          m_pin_locked = true;
        }
      }
      continue;
    }

    // Remove the Buffer from the Queue.
    if (num_buffers_processed)
    {
      std::array<ALuint, OAL_BUFFERS> unqueued_buffer_ids;
      alSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
      err = CheckALError("unqueuing buffers");

      num_buffers_queued -= num_buffers_processed;
    }

    // Control clock
    //ClockController();

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
      {
        // Release the lock if there is not enough samples
        if (m_pin_locked)
        {
          if (ReleaseMutex(m_pinput_pin->m_input_mutex))
          {
            m_pin_locked = false;
          }
        }
        continue;
      }

      // samples that will be buffered by OpenAL
      available_samples = available_samples / (num_buffers - num_buffers_queued);

      if (available_samples > OAL_MAX_FRAMES * STEREO_CHANNELS)
      {
        available_samples = OAL_MAX_FRAMES * STEREO_CHANNELS;
      }

      if (available_samples % STEREO_CHANNELS != 0)
      {
        --available_samples;
      }

      // Get data from queue
      std::array<__int16, OAL_MAX_FRAMES * STEREO_CHANNELS> short_data;

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
      OutputDebugString(L"Buffer under-flow\n");
    }
  }
}

#endif  // _WIN32
