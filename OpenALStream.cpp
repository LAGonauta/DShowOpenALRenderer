// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32

#include <numeric>

#include "BaseHeader.h"

void DebugPrintf(const wchar_t *str, ...)
{
  wchar_t buf[2048];

  va_list ptr;
  va_start(ptr, str);
  vswprintf_s(buf, 2048, str, ptr);

  OutputDebugString(buf);
}

void DebugPrintf(const char *str, ...)
{
  char buf[2048];

  va_list ptr;
  va_start(ptr, str);
  vsprintf_s(buf, 2048, str, ptr);

  OutputDebugStringA(buf);
}

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
}

COpenALStream::COpenALStream(LPUNKNOWN pUnk, HRESULT* phr, COpenALFilter* base_filter)
  : CBasicAudio(NAME("OpenAL Volume Setting"), pUnk),
  CBaseReferenceClock(NAME("OpenAL Stream Clock"), pUnk, phr),
  m_base_filter(base_filter)
{
  EXECUTE_ASSERT(SUCCEEDED(isValid()));

  EXECUTE_ASSERT(SUCCEEDED(OpenDevice()));
}

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

REFERENCE_TIME COpenALStream::GetPrivateTime()
{
  CAutoLock cObjectLock(this);

  REFERENCE_TIME clock = MILLISECONDS_TO_100NS_UNITS(timeGetTime());
  if (m_base_filter->m_State == State_Running)
  {
    REFERENCE_TIME sampleTime = getSampleTime();
    REFERENCE_TIME startTime = m_base_filter->m_tStart;
    if (sampleTime > 0)
      clock = sampleTime + startTime;
  }

  return clock;
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
  past_frequency = m_frequency;
  past_speaker_layout = m_speaker_layout;
  past_bitness = m_bitness;

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
  return S_OK;
}

STDMETHODIMP COpenALStream::StopDevice(void)
{
  Stop();

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
  m_total_played = 0;
  OutputDebugStringA("Stopped, cleared buffers.\n");

  return S_OK;
}

COpenALStream::SpeakerLayout COpenALStream::getSpeakerLayout()
{
  return m_speaker_layout;
}

COpenALStream::MediaBitness COpenALStream::getBitness()
{
  return m_bitness;
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

COpenALStream::SpeakerLayout COpenALStream::ChannelsToSpeakerLayout(size_t num_channels)
{
  switch (num_channels)
  {
  case 1:
    return SpeakerLayout::Mono;
  case 2:
    return SpeakerLayout::Stereo;
  case 4:
    return SpeakerLayout::Quad;
  case 6:
    return SpeakerLayout::Surround6;
  case 8:
    return SpeakerLayout::Surround8;
  default:
    throw std::invalid_argument("received invalid number of audio channels");
  }
}

COpenALStream::MediaBitness COpenALStream::WaveformatToBitness(const WAVEFORMATEX * wave_format)
{
  if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
  {
    auto format = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave_format);
    if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
    {
      return MediaBitness::bitfloat;
    }
    else if (format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
    {
      switch (format->Format.wBitsPerSample)
      {
      case 8:
        return MediaBitness::bit8;
      case 16:
        return MediaBitness::bit16;
      case 24:
        return MediaBitness::bit24;
      case 32:
        return MediaBitness::bit32;
      }
    }
  }
  else if (wave_format->wFormatTag == WAVE_FORMAT_PCM)
  {
    auto format = reinterpret_cast<const PCMWAVEFORMAT*>(wave_format);
    switch (format->wBitsPerSample)
    {
    case 8:
      return MediaBitness::bit8;
    case 16:
      return MediaBitness::bit16;
    case 24:
      return MediaBitness::bit24;
    case 32:
      return MediaBitness::bit32;
    }
  }
  throw std::invalid_argument("invalid media bitness");
}

HRESULT COpenALStream::ResetBuffer()
{
  palSourceStop(m_source);
  palSourcei(m_source, AL_BUFFER, 0);

  palDeleteBuffers(num_buffers, m_buffers.data());
  palGenBuffers(num_buffers, (ALuint*)m_buffers.data());
  //err = CheckALError("re-generating buffers");

  next_buffer = 0;
  num_buffers_queued = 0;

  return S_OK;
}

REFERENCE_TIME COpenALStream::getSampleTime()
{
  double total_played_ms = static_cast<double>(m_total_played * 1000 / m_frequency);

  // Add played
  float offset = 0;
  if (m_source)
  {
    palGetSourcef(m_source, AL_SEC_OFFSET, &offset);
    total_played_ms += offset * 1000;
  }

  //std::ostringstream string;
  //string << "Current start time: " << m_base_filter->m_tStart.GetUnits() << ". Buffered time in units of 100ns: " << static_cast<REFERENCE_TIME>(total_played_ms * UNITS / MILLISECONDS) << "." << std::endl;
  //OutputDebugStringA(string.str().c_str());

  return static_cast<REFERENCE_TIME>(total_played_ms * UNITS / MILLISECONDS);
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

//
// StartStreaming
//
// This is called when we start running state
//
HRESULT COpenALStream::StartStreaming()
{
  CAutoLock cAutoLock(this);

  // Are we already streaming

  if (m_bStreaming == true)
  {
    return NOERROR;
  }

  m_bStreaming = true;
  return NOERROR;
} // StartStreaming

  //
  // StopStreaming
  //
  // This is called when we stop streaming
  //
HRESULT COpenALStream::StopStreaming()
{
  CAutoLock cAutoLock(this);

  // Have we been stopped already

  if (m_bStreaming == false)
  {
    return NOERROR;
  }

  m_total_played = 0;
  m_bStreaming = false;
  return NOERROR;
} // StopStreaming

HRESULT COpenALStream::setMediaType(const CMediaType* pmt)
{
  auto format = reinterpret_cast<const WAVEFORMATEX*>(pmt->Format());
  HRESULT result = checkMediaType(format);
  if (result == S_OK)
  {
    m_speaker_layout = ChannelsToSpeakerLayout(format->nChannels);
    m_bitness = WaveformatToBitness(format);
    m_frequency = format->nSamplesPerSec;
    m_nChannels = format->nChannels;
    m_nSamplesPerSec = format->nSamplesPerSec;
    m_nBitsPerSample = format->wBitsPerSample;
    m_nBlockAlign = format->nBlockAlign;
    m_is_float = (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
    if (m_is_float == false && format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      auto formatex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
      if (formatex->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
      {
        m_is_float = true;
      }
    }
    return ResetBuffer();
  }

  return S_FALSE;
}

HRESULT COpenALStream::checkMediaType(const WAVEFORMATEX* wave_format)
{
  // Get supported layout
  auto supported_bitness = getSupportedBitness();

  // Get supported bitness
  auto supported_layouts = getSupportedSpeakerLayout();
  bool valid_channel_layout = false;
  bool valid_sample_type = false;

  // Normalize channels
  SpeakerLayout speaker_layout;
  try
  {
    speaker_layout = ChannelsToSpeakerLayout(wave_format->nChannels);
  }
  catch (const std::invalid_argument& e)
  {
    DebugPrintf(e.what());
    return S_FALSE;
  }

  if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
    [speaker_layout](SpeakerLayout layout) { return layout == speaker_layout; }))
  {
    valid_channel_layout = true;
  }
  else
  {
    return S_FALSE;
  }

  // Normalize bitness
  MediaBitness media_bitness;
  try
  {
    media_bitness = WaveformatToBitness(wave_format);
  }
  catch (const std::invalid_argument& e)
  {
    DebugPrintf(e.what());
    return S_FALSE;
  }

  if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
    [media_bitness](MediaBitness bitness) { return bitness == media_bitness; }))
  {
    valid_sample_type = true;
  }

  if (valid_channel_layout && valid_sample_type)
  {
    return S_OK;
  }

  return S_FALSE;
}

void COpenALStream::PushToDevice(IMediaSample *pMediaSample)
{
  //waitobject
  constexpr size_t bits_per_byte = 8;
  BYTE *pWave = nullptr;
  int  nBytes = 0;
  ALint state = 0;

  ASSERT(pMediaSample);
  if (!pMediaSample)
    return;

  pMediaSample->GetPointer(&pWave);
  ASSERT(pWave != nullptr);

  nBytes = pMediaSample->GetActualDataLength();
  nBytes = nBytes - nBytes % (m_nChannels * m_nBitsPerSample / bits_per_byte);
  if (nBytes == 0)
  {
    return;
  }

  // Check if any buffer was played and release from the queue, else wait.
  if (num_buffers_queued == num_buffers)
  {
    int num_buffers_processed = 0;
    while (true)
    {
      palGetSourcei(m_source, AL_BUFFERS_PROCESSED, &num_buffers_processed);
      palGetSourcei(m_source, AL_SOURCE_STATE, &state);
      // Buffer underrun
      if (m_bStreaming && state != AL_PLAYING)
      {
        palSourcePlay(m_source);
      }
      if (num_buffers_processed > 0)
      {
        std::vector<ALuint> unqueued_buffer_ids(num_buffers);
        palSourceUnqueueBuffers(m_source, num_buffers_processed, unqueued_buffer_ids.data());
        //err = CheckALError("unqueuing buffers");

        num_buffers_queued -= num_buffers_processed;
        for (int i = 0; i < num_buffers_processed; ++i)
        {
          m_total_played += m_buffer_size.front();
          m_buffer_size.pop_front();
        }
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  // Push to buffer and release
  palBufferData(m_buffers[next_buffer],
    palGetEnumValue(GenerateFormatString(m_speaker_layout, m_bitness).c_str()),
    pWave,
    nBytes,
    m_frequency);
  palSourceQueueBuffers(m_source, 1, &m_buffers[next_buffer]);
  num_buffers_queued++;
  next_buffer = (next_buffer + 1) % num_buffers;

  size_t m_rendered_samples = nBytes / m_nChannels / m_nBitsPerSample * bits_per_byte;
  m_buffer_size.push_back(m_rendered_samples);
  {
    std::ostringstream string;
    size_t samples = std::accumulate(m_buffer_size.begin(), m_buffer_size.end(), 0);
    string << "Buffer sample size: " << samples << ". "; // pushed_bytes / m_nChannels / m_nBitsPerSample * bits_per_byte << " frames.\n";
    string << "Seconds buffered: " << static_cast<double>(samples) / m_frequency << ".\n";
    OutputDebugStringA(string.str().c_str());
  }

  palGetSourcei(m_source, AL_SOURCE_STATE, &state);
  if (m_bStreaming && state != AL_PLAYING)
  {
    // Buffer underrun occurred, resume playback
    palSourcePlay(m_source);
    //err = CheckALError("occurred resuming playback");
    OutputDebugStringA("Buffer underrun\n");
    {
      std::ostringstream string;
      string << "Buffers queued: " << num_buffers_queued << "." << std::endl;
      OutputDebugStringA(string.str().c_str());
    }
  }

  //std::ostringstream string;
  //string << "Size of the queue: " << pushed_bytes / m_nChannels / m_nBitsPerSample * bits_per_byte << " frames.\n";
  //OutputDebugStringA(string.str().c_str());
} // CopyWaveform

HRESULT COpenALStream::Receive(IMediaSample * pSample)
{
  CheckPointer(pSample, E_POINTER);
  CAutoLock cAutoLock(this);
  ASSERT(pSample != nullptr);

  REFERENCE_TIME tStart, tStop;
  pSample->GetTime(&tStart, &tStop);

  // Ignore zero-length samples
  if ((m_LastMediaSampleSize = pSample->GetActualDataLength()) == 0)
    return NOERROR;

  if (m_bStreaming == true)
  {
    PushToDevice(pSample);

    return NOERROR;
  }

  return NOERROR;
}
#endif  // _WIN32