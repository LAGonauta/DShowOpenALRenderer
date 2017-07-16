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
#include "OpenALAudioRenderer.h"

COpenALStream::~COpenALStream(void)
{
  CloseDevice();
}

COpenALStream::COpenALStream(CMixer* audioMixer, LPUNKNOWN pUnk, HRESULT * phr)
  : CBaseReferenceClock(NAME("OpenAL Stream Clock"), pUnk, phr),
    CBasicAudio(L"OpenAL Volume Setting", pUnk),
    m_pCurrentRefClock(0), m_pPrevRefClock(0)
{
  EXECUTE_ASSERT(SUCCEEDED(OpenDevice()));

  m_mixer = audioMixer;

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

REFERENCE_TIME COpenALStream::GetPrivateTime()
{
  CAutoLock cObjectLock(this);

  /* If the clock has wrapped then the current time will be less than
  * the last time we were notified so add on the extra milliseconds
  *
  * The time period is long enough so that the likelihood of
  * successive calls spanning the clock cycle is not considered.
  */

  // This returns the current time in ms according to our special clock.  If
  // we used timeGetTime() here, our clock would run normally.
  //DWORD dwTime = MetGetTime();
  DWORD dwTime = timeGetTime();
  {
    REFERENCE_TIME delta = REFERENCE_TIME(dwTime) - REFERENCE_TIME(m_dwPrevSystemTime);
    if (dwTime < m_dwPrevSystemTime)
      delta += REFERENCE_TIME(UINT_MAX) + 1;

    m_dwPrevSystemTime = dwTime;

    delta *= (UNITS / MILLISECONDS);
    m_rtPrivateTime += delta;
  }

  return m_rtPrivateTime;
}

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

  int spike = 0;

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

std::vector<std::string> GetAllDevices()
{
  std::vector<std::string> devices_names_list;
  ALint device_index = 0;
  const ALchar* device_names = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

  while (device_names && *device_names)
  {
    std::string name = device_names;
    devices_names_list.push_back(name);
    device_index++;
    device_names += strlen(device_names) + 1;
  }

  return devices_names_list;
}

STDMETHODIMP COpenALStream::OpenDevice(void)
{
  if (!alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
  {
    printf_s("OpenAL: can't find sound devices");
    return E_FAIL;
  }

  const char* default_device = alcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);

  if (!strlen(default_device))
  {
    printf_s("No device found.\n");
    return E_FAIL;
  }

  std::vector<std::string> devices = GetAllDevices();

  printf_s("Found OpenAL device %s", devices[0].c_str());

  ALCdevice* device = alcOpenDevice(devices[0].c_str());
  if (!device)
  {
    printf_s("OpenAL: can't open device %s", devices[0].c_str());
    return E_FAIL;
  }

  ALCcontext* context = alcCreateContext(device, nullptr);
  if (!context)
  {
    alcCloseDevice(device);
    printf_s("OpenAL: can't create context for device %s", devices[0].c_str());
    return E_FAIL;
  }

  alcMakeContextCurrent(context);
  return S_OK;
}

STDMETHODIMP COpenALStream::CloseDevice(void)
{
  StopDevice();
  Destroy();

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

// Code from sanear
STDMETHODIMP COpenALStream::put_Volume(long volume)
{
  if (volume < -10000 || volume > 0)
    return E_FAIL;

  float f = (volume == 0) ?
    1.0f : pow(10.0f, (float)volume / 2000.0f);

  m_volume = f;
  alSourcef(m_source, AL_GAIN, m_volume);

  return S_OK;
}

STDMETHODIMP COpenALStream::get_Volume(long * pVolume)
{
  CheckPointer(pVolume, E_POINTER);

  float* f = &m_volume;
  if (alIsSource(m_source))
  {
    alGetSourcef(m_source, AL_GAIN, f);
  }

  *pVolume = (*f == 1.0f) ?
    0 : (long)(log10(*f) * 2000.0f);

  ASSERT(*pVolume <= 0 && *pVolume >= -10000);

  return S_OK;
}

STDMETHODIMP COpenALStream::put_Balance(long balance)
{
  if (balance < -10000 || balance > 10000)
  {
    return E_FAIL;
  }

  m_fake_balance = balance;

  return S_OK;
}

STDMETHODIMP COpenALStream::get_Balance(long* pBalance)
{
  CheckPointer(pBalance, E_POINTER);

  *pBalance = m_fake_balance;

  ASSERT(*pBalance >= -10000 && *pBalance <= 10000);

  return S_OK;
}

void COpenALStream::Destroy()
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

ALenum COpenALStream::CheckALError(std::wstring desc)
{
  ALenum err = alGetError();

  if (err != AL_NO_ERROR)
  {
    std::wstring type;

    switch (err)
    {
    case AL_INVALID_NAME:
      type = L"AL_INVALID_NAME";
      break;
    case AL_INVALID_ENUM:
      type = L"AL_INVALID_ENUM";
      break;
    case AL_INVALID_VALUE:
      type = L"AL_INVALID_VALUE";
      break;
    case AL_INVALID_OPERATION:
      type = L"AL_INVALID_OPERATION";
      break;
    case AL_OUT_OF_MEMORY:
      type = L"AL_OUT_OF_MEMORY";
      break;
    default:
      type = L"UNKNOWN_ERROR";
      break;
    }

    wchar_t string_buf[1024] = { 0 };
    swprintf_s(string_buf, sizeof(string_buf), L"Error %s: %08x %s\n", desc.c_str(), err, type.c_str());
    OutputDebugString(string_buf);
  }

  return err;
}

HRESULT COpenALStream::Pause()
{
  ALint state = 0;
  alGetSourcei(m_source, AL_SOURCE_STATE, &state);
  if (state == AL_PAUSED)
  {
    return S_OK;
  }
  else if (state == AL_PLAYING)
  {
    alSourcePause(m_source);
    return S_OK;
  }

  return E_FAIL;
}

HRESULT COpenALStream::Stop()
{
  alSourceStop(m_source);

  return S_OK;
}

HRESULT COpenALStream::setSpeakerLayout(SpeakerLayout layout)
{
  m_speaker_layout = layout;
  return S_OK;
}

COpenALStream::SpeakerLayout COpenALStream::getSpeakerLayout()
{
  return m_speaker_layout;
}

HRESULT COpenALStream::setFrequency(uint32_t frequency)
{
  m_frequency = frequency;
  return S_OK;
}

uint32_t COpenALStream::getFrequency()
{
  return m_frequency;
}

HRESULT COpenALStream::setBitness(MediaBitness bitness)
{
  m_bitness = bitness;
  return S_OK;
}

COpenALStream::MediaBitness COpenALStream::getBitness()
{
  return m_bitness;
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

static bool IsCreativeXFi()
{
  return strstr(alGetString(AL_RENDERER), "X-Fi") != nullptr;
}

void COpenALStream::SoundLoop()
{
  uint32_t past_frequency = m_frequency;
  SpeakerLayout past_speaker_layout = m_speaker_layout;
  MediaBitness past_bitness = m_bitness;

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
  err = CheckALError(L"generating buffers");

  // Generate a Source to playback the Buffers
  alGenSources(1, &m_source);
  err = CheckALError(L"generating sources");

  // Set the default sound volume as saved in the config file.
  alSourcef(m_source, AL_GAIN, m_volume);

  // TODO: Error handling
  // ALenum err = alGetError();

  unsigned int next_buffer = 0;
  unsigned int num_buffers_queued = 0;
  ALint state = 0;

  std::vector<int16_t> short_data;
  std::vector<int32_t> long_data;

  while (m_run_thread)
  {
    if (m_mixer->IsStreaming())
    {
      // Check if stream changed frequency, bitness or channel setup
      if (past_frequency != m_frequency || past_bitness != m_bitness || past_speaker_layout != m_speaker_layout)
      {
        // Stop source and clean-up buffers
        alSourceStop(m_source);
        alSourcei(m_source, AL_BUFFER, 0);

        alDeleteBuffers(OAL_BUFFERS, m_buffers.data());
        alGenBuffers(OAL_BUFFERS, (ALuint*)m_buffers.data());
        err = CheckALError(L"re-generating buffers");

        next_buffer = 0;
        num_buffers_queued = 0;

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

        past_frequency = m_frequency;
        past_bitness = m_bitness;
        past_speaker_layout = m_speaker_layout;
      }

      // Block until we have a free buffer
      int num_buffers_processed = 0;
      alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_buffers_processed);
      alGetSourcei(m_source, AL_SOURCE_STATE, &state);
      if ((num_buffers_queued == OAL_BUFFERS && !num_buffers_processed) || state == AL_PAUSED)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      // Remove the Buffer from the Queue.
      if (num_buffers_processed)
      {
        std::array<ALuint, OAL_BUFFERS> unqueued_buffer_ids;
        alSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
        err = CheckALError(L"unqueuing buffers");

        num_buffers_queued -= num_buffers_processed;
      }

      // Control clock
      //ClockController();

      size_t min_frames = 64;

      size_t available_frames = 0;
      if (m_speaker_layout == Surround6)
      {
        if (m_bitness == bit16)
        {
          available_frames = m_mixer->Mix(&short_data, frames_per_buffer);

          if (available_frames < min_frames)
          {
            continue;
          }

          alBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN16, short_data.data(),
            static_cast<ALsizei>(available_frames) * FRAME_SURROUND_SHORT, m_frequency);
        }
        else if (m_bitness = bit32)
        {
          available_frames = m_mixer->Mix(&long_data, frames_per_buffer);

          if (!available_frames)
          {
            continue;
          }

          alBufferData(m_buffers[next_buffer], AL_FORMAT_51CHN32, long_data.data(),
            static_cast<ALsizei>(available_frames) * FRAME_SURROUND_INT32, m_frequency);
        }
      }
      else
      {
        if (m_bitness == bit16)
        {
          available_frames = m_mixer->Mix(&short_data, frames_per_buffer);

          if (!available_frames)
          {
            continue;
          }

          alBufferData(m_buffers[next_buffer], AL_FORMAT_STEREO16, short_data.data(),
            static_cast<ALsizei>(available_frames) * FRAME_STEREO_SHORT, m_frequency);
        }
        else if (m_bitness = bit32)
        {
          available_frames = m_mixer->Mix(&long_data, frames_per_buffer);

          if (!available_frames)
          {
            continue;
          }

          alBufferData(m_buffers[next_buffer], AL_FORMAT_STEREO32, long_data.data(),
            static_cast<ALsizei>(available_frames) * FRAME_STEREO_INT32, m_frequency);
        }
      }
      err = CheckALError(L"buffering data");

      alSourceQueueBuffers(m_source, 1, &m_buffers[next_buffer]);
      err = CheckALError(L"queuing buffers");

      num_buffers_queued++;
      next_buffer = (next_buffer + 1) % OAL_BUFFERS;

      alGetSourcei(m_source, AL_SOURCE_STATE, &state);
      if (state != AL_PLAYING)
      {
        // Buffer underrun occurred, resume playback
        alSourcePlay(m_source);
        err = CheckALError(L"occurred resuming playback");
        OutputDebugString(L"Buffer underrun\n");
      }
    }
    else
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
  }
}

#endif  // _WIN32
