// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32

#include <windows.h>
#include <sstream>
#include <thread>
#include <vector>

#include "OpenALStream.h"
#include "OpenALAudioRenderer.h"

static HMODULE s_openal_dll = nullptr;

#define OPENAL_API_VISIT(X)                                                                        \
  X(alBufferData)                                                                                  \
  X(alcCloseDevice)                                                                                \
  X(alcCreateContext)                                                                              \
  X(alcDestroyContext)                                                                             \
  X(alcGetContextsDevice)                                                                          \
  X(alcGetCurrentContext)                                                                          \
  X(alcGetString)                                                                                  \
  X(alcIsExtensionPresent)                                                                         \
  X(alcMakeContextCurrent)                                                                         \
  X(alcOpenDevice)                                                                                 \
  X(alDeleteBuffers)                                                                               \
  X(alDeleteSources)                                                                               \
  X(alGenBuffers)                                                                                  \
  X(alGenSources)                                                                                  \
  X(alGetError)                                                                                    \
  X(alGetSourcei)                                                                                  \
  X(alGetString)                                                                                   \
  X(alIsExtensionPresent)                                                                          \
  X(alSourcef)                                                                                     \
  X(alSourcei)                                                                                     \
  X(alSourcePlay)                                                                                  \
  X(alSourceQueueBuffers)                                                                          \
  X(alSourceStop)                                                                                  \
  X(alSourceUnqueueBuffers)                                                                        \
  X(alGetEnumValue)                                                                                \
  X(alIsSource)                                                                                    \
  X(alGetSourcef)

// Create func_t function pointer type and declare a nullptr-initialized static variable of that
// type named "pfunc".
#define DYN_FUNC_DECLARE(func)                                                                     \
  typedef decltype(&func) func##_t;                                                                \
  static func##_t p##func = nullptr;

// Attempt to load the function from the given module handle.
#define OPENAL_FUNC_LOAD(func)                                                                     \
  p##func = (func##_t)::GetProcAddress(s_openal_dll, #func);                                       \
  if (!p##func)                                                                                    \
  {                                                                                                \
    return false;                                                                                  \
  }

OPENAL_API_VISIT(DYN_FUNC_DECLARE);

static bool InitFunctions()
{
  OPENAL_API_VISIT(OPENAL_FUNC_LOAD);
  return true;
}

static bool InitLibrary()
{
  if (s_openal_dll)
    return true;

  s_openal_dll = ::LoadLibrary(TEXT("openal32.dll"));
  if (!s_openal_dll)
    return false;

  if (!InitFunctions())
  {
    ::FreeLibrary(s_openal_dll);
    s_openal_dll = nullptr;
    return false;
  }

  return true;
}

STDMETHODIMP COpenALStream::isValid()
{
  if (InitLibrary())
  {
    return S_OK;
  }
  else
  {
    return E_FAIL;
  }
}

COpenALStream::~COpenALStream(void)
{
  CloseDevice();

  // Finally Terminate thread
  if (m_thread.joinable())
  {
    m_thread.join();
  }
}

COpenALStream::COpenALStream(CMixer* audioMixer, LPUNKNOWN pUnk, HRESULT * phr)
  : CBaseReferenceClock(NAME("OpenAL Stream Clock"), pUnk, phr),
  CBasicAudio(L"OpenAL Volume Setting", pUnk),
  m_pCurrentRefClock(0), m_pPrevRefClock(0)
{
  EXECUTE_ASSERT(SUCCEEDED(isValid()));

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
  const ALchar* device_names = palcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

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
  if (!palcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT"))
  {
    OutputDebugStringA("OpenAL: can't find sound devices\n");
    return E_FAIL;
  }

  const char* default_device = palcGetString(nullptr, ALC_DEFAULT_DEVICE_SPECIFIER);

  if (!strlen(default_device))
  {
    OutputDebugStringA("No device found.\n");
    return E_FAIL;
  }

  std::vector<std::string> devices = GetAllDevices();

  {
    std::ostringstream string;
    string << "Found OpenAL device \"" << devices[0].c_str() << "\"." << std::endl;
    OutputDebugStringA(string.str().c_str());
  }

  ALCdevice* device = palcOpenDevice(devices[0].c_str());
  if (!device)
  {
    std::ostringstream string;
    string << "OpenAL: can't open device " << devices[0].c_str() << std::endl;
    OutputDebugStringA(string.str().c_str());
    return E_FAIL;
  }

  ALCcontext* context = palcCreateContext(device, nullptr);
  if (!context)
  {
    palcCloseDevice(device);
    std::ostringstream string;
    string << "OpenAL: can't create context for device " << devices[0].c_str() << std::endl;
    OutputDebugStringA(string.str().c_str());
    return E_FAIL;
  }

  palcMakeContextCurrent(context);
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
    // Terminate older thread, if it exists
    if (m_thread.joinable())
    {
      m_thread.join();
    }

    m_run_thread = true;
    m_thread = std::thread(&COpenALStream::SoundLoop, this);
  }

  return S_OK;
}

STDMETHODIMP COpenALStream::StopDevice(void)
{
  m_run_thread = false;

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
  palSourcef(m_source, AL_GAIN, m_volume);

  return S_OK;
}

STDMETHODIMP COpenALStream::get_Volume(long* pVolume)
{
  CheckPointer(pVolume, E_POINTER);

  float f = m_volume;
  if (palIsSource(m_source))
  {
    palGetSourcef(m_source, AL_GAIN, &f);
  }

  *pVolume = (f == 1.0f) ?
    0 : (long)(log10(f) * 2000.0f);

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
  ALCcontext* context = palcGetCurrentContext();

  if (context != nullptr)
  {
    if (palIsSource(m_source))
    {
      palSourceStop(m_source);
      palSourcei(m_source, AL_BUFFER, 0);

      // Clean up buffers and sources
      palDeleteSources(1, &m_source);
      m_source = 0;
      palDeleteBuffers(num_buffers, m_buffers.data());
    }

    ALCdevice* device = palcGetContextsDevice(context);

    palcMakeContextCurrent(nullptr);
    palcDestroyContext(context);
    palcCloseDevice(device);
  }
}

ALenum COpenALStream::CheckALError(std::string desc)
{
  ALenum err = palGetError();

  if (err != AL_NO_ERROR)
  {
    std::ostringstream string_stream;
    string_stream << "Error " << desc.c_str() << ": " << palGetString(err) << std::endl;
    OutputDebugStringA(string_stream.str().c_str());
  }

  return err;
}

HRESULT COpenALStream::Stop()
{
  palSourceStop(m_source);
  palSourcei(m_source, AL_BUFFER, 0);
  m_total_buffered = 0;
  OutputDebugStringA("Stopped, cleared buffers.\n");

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
    palSourcef(m_source, AL_GAIN, m_volume);
}

static bool IsCreativeXFi()
{
  return strstr(palGetString(AL_RENDERER), "X-Fi") != nullptr;
}

std::vector<COpenALStream::MediaBitness> COpenALStream::getSupportedBitness()
{
  std::vector<MediaBitness> supported_bitness;

  if (palIsExtensionPresent("AL_EXT_float32"))
  {
    supported_bitness.push_back(bitfloat);
  }

  if (IsCreativeXFi())
  {
    supported_bitness.push_back(bit32);
  }

  // All implementation support 16-bit and 8-bit
  supported_bitness.push_back(bit16);
  supported_bitness.push_back(bit8);

  return supported_bitness;
}

std::vector<COpenALStream::SpeakerLayout> COpenALStream::getSupportedSpeakerLayout()
{
  std::vector<SpeakerLayout> supported_layouts;
  bool surround_capable = palIsExtensionPresent("AL_EXT_MCFORMATS") || IsCreativeXFi();

  if (surround_capable)
  {
    supported_layouts.push_back(Surround8);
    supported_layouts.push_back(Surround6);
  }

  supported_layouts.push_back(Quad);
  supported_layouts.push_back(Stereo);
  supported_layouts.push_back(Mono);

  return supported_layouts;
}

REFERENCE_TIME COpenALStream::getSampleTime()
{
  size_t total_played = m_total_buffered * 1000;
  total_played /= m_frequency;

  // Subtract played
  float offset = 0;
  if (m_source)
  {
    palGetSourcef(m_source, AL_SEC_OFFSET, &offset);
    offset *= 1000;
    if (offset < total_played)
      total_played -= static_cast<size_t>(offset);
  }

  std::ostringstream string;
  string << "Buffered time in milliseconds: " << total_played << "." << std::endl;
  OutputDebugStringA(string.str().c_str());

  return total_played;
}

HRESULT COpenALStream::resetSampleTime()
{
  m_total_buffered = 0;
  return S_OK;
}

std::string GenerateFormatString(COpenALStream::SpeakerLayout speaker_layout, COpenALStream::MediaBitness bitness)
{
  std::string result = "AL_FORMAT_";

  switch (speaker_layout)
  {
  case COpenALStream::SpeakerLayout::Mono:
    result.append("MONO");
    break;
  case COpenALStream::SpeakerLayout::Stereo:
    result.append("STEREO");
    break;
  case COpenALStream::SpeakerLayout::Quad:
    result.append("QUAD");
    break;
  case COpenALStream::SpeakerLayout::Surround6:
    result.append("51CHN");
    break;
  case COpenALStream::SpeakerLayout::Surround8:
    result.append("71CHN");
    break;
  }

  switch (bitness)
  {
  case COpenALStream::MediaBitness::bit8:
    result.append("8");
    break;
  case COpenALStream::MediaBitness::bit16:
    result.append("16");
    break;
  case COpenALStream::MediaBitness::bit24:
    //result.append("QUAD");
    break;
  case COpenALStream::MediaBitness::bit32:
    result.append("32");
    break;
  case COpenALStream::MediaBitness::bitfloat:
    if (speaker_layout == COpenALStream::SpeakerLayout::Stereo ||
      speaker_layout == COpenALStream::SpeakerLayout::Mono)
    {
      result.append("_FLOAT32");
    }
    else
    {
      result.append("32");
    }
    break;
  }

  return result;
}

size_t GetFrameSize(COpenALStream::SpeakerLayout speaker_layout, COpenALStream::MediaBitness bitness)
{
  size_t num_channels = 0;
  size_t element_size = 0;
  switch (bitness)
  {
  case COpenALStream::MediaBitness::bit8:
    element_size = sizeof(ALbyte);
    break;
  case COpenALStream::MediaBitness::bit16:
    element_size = sizeof(ALshort);
    break;
  case COpenALStream::MediaBitness::bit24:
    element_size = sizeof(ALint);
    break;
  case COpenALStream::MediaBitness::bit32:
    element_size = sizeof(ALint);
    break;
  case COpenALStream::MediaBitness::bitfloat:
    element_size = sizeof(ALfloat);
    break;
  }

  switch (speaker_layout)
  {
  case COpenALStream::SpeakerLayout::Mono:
    num_channels = 1;
    break;
  case COpenALStream::SpeakerLayout::Stereo:
    num_channels = 2;
    break;
  case COpenALStream::SpeakerLayout::Quad:
    num_channels = 4;
    break;
  case COpenALStream::SpeakerLayout::Surround6:
    num_channels = 6;
    break;
  case COpenALStream::SpeakerLayout::Surround8:
    num_channels = 8;
    break;
  }

  return num_channels * element_size;
}

void COpenALStream::SoundLoop()
{
  uint32_t past_frequency = m_frequency;
  SpeakerLayout past_speaker_layout = m_speaker_layout;
  MediaBitness past_bitness = m_bitness;

  bool float32_capable = palIsExtensionPresent("AL_EXT_float32");
  bool surround_capable = palIsExtensionPresent("AL_EXT_MCFORMATS") || IsCreativeXFi();

  // As there is no extension to check for 32-bit fixed point support
  // and we know that only a X-Fi with hardware OpenAL supports it,
  // we just check if one is being used.
  bool fixed32_capable = IsCreativeXFi();

  uint32_t frames_per_buffer;
  // Can't have zero samples per buffer
  if (m_latency > 0)
  {
    frames_per_buffer = m_frequency / 1000 * m_latency / num_buffers;
  }
  else
  {
    frames_per_buffer = m_frequency / 1000 * 1 / num_buffers;
  }

  std::ostringstream string;
  string << "Using " << num_buffers << " buffers, each with " << frames_per_buffer <<
    " audio frames for a total of " << frames_per_buffer * num_buffers << " frames." << std::endl;
  OutputDebugStringA(string.str().c_str());
  string.clear();

  // Should we make these larger just in case the mixer ever sends more samples
  // than what we request?
  m_buffers.resize(num_buffers);
  m_source = 0;

  // Clear error state before querying or else we get false positives.
  ALenum err = palGetError();

  // Generate some AL Buffers for streaming
  palGenBuffers(num_buffers, (ALuint*)m_buffers.data());
  err = CheckALError("generating buffers");

  // Generate a Source to playback the Buffers
  palGenSources(1, &m_source);
  err = CheckALError("generating sources");

  // Set the default sound volume as saved in the config file.
  palSourcef(m_source, AL_GAIN, m_volume);

  // TODO: Error handling
  // ALenum err = alGetError();

  unsigned int next_buffer = 0;

  ALint state = 0;

  std::vector<int8_t> byte_data;
  while (m_run_thread)
  {
    if (m_mixer->IsStreaming())
    {
      // Check if stream changed frequency, bitness or channel setup
      if (past_frequency != m_frequency || past_bitness != m_bitness || past_speaker_layout != m_speaker_layout)
      {
        // Stop source and clean-up buffers
        palSourceStop(m_source);
        palSourcei(m_source, AL_BUFFER, 0);

        palDeleteBuffers(num_buffers, m_buffers.data());
        palGenBuffers(num_buffers, (ALuint*)m_buffers.data());
        err = CheckALError("re-generating buffers");

        next_buffer = 0;
        num_buffers_queued = 0;

        if (m_latency > 0)
        {
          frames_per_buffer = m_frequency / 1000 * m_latency / num_buffers;
        }
        else
        {
          frames_per_buffer = m_frequency / 1000 * 1 / num_buffers;
        }

        past_frequency = m_frequency;
        past_bitness = m_bitness;
        past_speaker_layout = m_speaker_layout;
      }

      // Block until we have a free buffer
      int num_buffers_processed = 0;
      palGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_buffers_processed);
      palGetSourcei(m_source, AL_SOURCE_STATE, &state);
      if (num_buffers_queued == num_buffers && !num_buffers_processed)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }

      // Remove the Buffer from the Queue.
      if (num_buffers_processed)
      {
        std::vector<ALuint> unqueued_buffer_ids(num_buffers);
        palSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
        err = CheckALError("unqueuing buffers");

        num_buffers_queued -= num_buffers_processed;
      }

      // Control clock
      //ClockController();

      size_t available_frames = 0;
      switch (m_bitness)
      {
      case bit8:
        available_frames = m_mixer->Mix(&byte_data, frames_per_buffer, 1);
        break;
      case bit16:
        available_frames = m_mixer->Mix(&byte_data, frames_per_buffer, 2);
        break;
      case bit32:
        available_frames = m_mixer->Mix(&byte_data, frames_per_buffer, 4);
        break;
      case bitfloat:
        available_frames = m_mixer->Mix(&byte_data, frames_per_buffer, 4);
        break;
      }

      if (!available_frames)
      {
        continue;
      }

      palBufferData(m_buffers[next_buffer],
        palGetEnumValue(GenerateFormatString(m_speaker_layout, m_bitness).c_str()),
        byte_data.data(),
        static_cast<ALsizei>(available_frames) * GetFrameSize(m_speaker_layout, m_bitness),
        m_frequency);

      err = CheckALError("buffering data");

      palSourceQueueBuffers(m_source, 1, &m_buffers[next_buffer]);
      err = CheckALError("queuing buffers");

      m_total_buffered += available_frames;

      num_buffers_queued++;
      next_buffer = (next_buffer + 1) % num_buffers;

      palGetSourcei(m_source, AL_SOURCE_STATE, &state);
      if (state != AL_PLAYING)
      {
        // Buffer underrun occurred, resume playback
        palSourcePlay(m_source);
        err = CheckALError("occurred resuming playback");
        OutputDebugStringA("Buffer underrun\n");
        {
          std::ostringstream string;
          string << "Buffers queued: " << num_buffers_queued << "." << std::endl;
          OutputDebugStringA(string.str().c_str());
        }
      }
    }
    else
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

#endif  // _WIN32