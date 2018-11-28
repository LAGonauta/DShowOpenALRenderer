//------------------------------------------------------------------------------
// File: Scope.cpp
//
// Desc: DirectShow sample code - illustration of an audio oscilloscope.  It
//       shows the waveform graphically as the audio is received by the filter.
//       The filter is a renderer that can be placed wherever the normal
//       runtime renderer goes.  It has a single input pin that accepts a
//       number of different audio formats and renders the data appropriately.
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//------------------------------------------------------------------------------

#include <streams.h>
#include <initguid.h>
#include <mmreg.h>
#include <sstream>
#include <algorithm>

#include "OpenALAudioRenderer.h"
#include "OpenALStream.h"

// Setup data

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
  &MEDIATYPE_Audio,           // Major type
  &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins =
{
  L"Input",                   // Pin string name
  TRUE,                       // Is it rendered
  FALSE,                      // Is it an output
  FALSE,                      // Allowed zero pins
  FALSE,                      // Allowed many
  &CLSID_NULL,                // Connects to filter
  nullptr,                    // Connects to pin
  1,                          // Number of pins types
  &sudPinTypes };             // Pin information

const AMOVIESETUP_FILTER sudOALRend =
{
  &CLSID_OALRend,             // Filter CLSID
  L"OpenAL Renderer",         // String name
  MERIT_DO_NOT_USE,           // Filter merit
  1,                          // Number pins
  &sudPins                    // Pin details
};

// List of class IDs and creator functions for class factory

CFactoryTemplate g_Templates[] = {
  { L"OpenAL Renderer"
  , &CLSID_OALRend
  , (LPFNNewCOMObject)COpenALFilter::CreateInstance
  , nullptr
  , &sudOALRend }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

//
// CreateInstance
//
// This goes in the factory template table to create new instances
//
CUnknown * WINAPI COpenALFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
  return new COpenALFilter(pUnk, phr);
} // CreateInstance

  //
  // Constructor
  //
  // Create the filter, scope window, and input pin
  //
#pragma warning(disable:4355 4127)

COpenALFilter::COpenALFilter(LPUNKNOWN pUnk, HRESULT *phr) :
  CBaseFilter(NAME("OpenAL Renderer"), pUnk, (CCritSec *)this, CLSID_OALRend),
  m_mixer(NAME("OpenAL Renderer"), this, phr)
{
  ASSERT(phr);

  // Create the single input pin
  m_pInputPin = new CAudioInputPin(this, phr, L"Audio Input Pin");
  if (m_pInputPin == nullptr)
  {
    if (phr)
      *phr = E_OUTOFMEMORY;
  }

  m_openal_device = new COpenALStream(&m_mixer, static_cast<IBaseFilter*>(this), phr);
} // (Constructor)

  //
  // Destructor
  //
COpenALFilter::~COpenALFilter()
{
  // Delete the contained interfaces

  ASSERT(m_pInputPin);
  delete m_pInputPin;
  m_pInputPin = nullptr;

  ASSERT(m_openal_device);
  delete m_openal_device;
  m_openal_device = nullptr;
} // (Destructor)

  //
  // GetPinCount
  //
  // Return the number of input pins we support
  //
int COpenALFilter::GetPinCount()
{
  return 1;
} // GetPinCount

  //
  // GetPin
  //
  // Return our single input pin - not addrefed
  //
CBasePin *COpenALFilter::GetPin(int n)
{
  // We only support one input pin and it is numbered zero

  ASSERT(n == 0);
  if (n != 0)
  {
    return nullptr;
  }

  return m_pInputPin;
} // GetPin

  //
  // JoinFilterGraph
  //
  // Show our window when we join a filter graph
  //   - and hide it when we are annexed from it
  //
STDMETHODIMP COpenALFilter::JoinFilterGraph(IFilterGraph *pGraph, LPCWSTR pName)
{
  HRESULT hr = CBaseFilter::JoinFilterGraph(pGraph, pName);
  if (FAILED(hr))
  {
    return hr;
  }

  return hr;
} // JoinFilterGraph

HRESULT COpenALFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
  CheckPointer(ppv, E_POINTER);

  if (riid == IID_IUnknown)
  {
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
  }

  if (riid == IID_IReferenceClock || riid == IID_IReferenceClockTimerControl)
  {
    return GetInterface(static_cast<IReferenceClock*>(m_openal_device), ppv);
  }

  if (riid == IID_IBasicAudio)
  {
    return GetInterface(static_cast<IBasicAudio*>(m_openal_device), ppv);
  }

  if (riid == IID_IMediaSeeking)
  {
    if (m_seeking == nullptr)
    {
      // Actually should be true, but still not implemented
      HRESULT hr = CreatePosPassThru(GetOwner(), TRUE, m_pInputPin, &m_seeking);
      if (FAILED(hr))
      {
        return hr;
      }
    }
    return m_seeking->QueryInterface(riid, ppv);
  }

  return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
}

//
  // Stop
  //
  // Switch the filter into stopped mode.
  //
STDMETHODIMP COpenALFilter::Stop()
{
  CAutoLock lock(this);

  if (m_State != State_Stopped)
  {
    // Pause the device if we were running
    if (m_State == State_Running)
    {
      HRESULT hr = Pause();
      if (FAILED(hr))
      {
        return hr;
      }
    }

    DbgLog((LOG_TRACE, 1, TEXT("Stopping....")));

    // Base class changes state and tells pin to go to inactive
    // the pin Inactive method will decommit our allocator which
    // we need to do before closing the device

    HRESULT hr = CBaseFilter::Stop();
    if (FAILED(hr))
    {
      return hr;
    }
  }

  return NOERROR;
} // Stop

  //
  // Pause
  //
  // Override Pause to stop the window streaming
  //
STDMETHODIMP COpenALFilter::Pause()
{
  CAutoLock lock(this);

  // Check we can PAUSE given our current state

  if (m_State == State_Running)
  {
    m_mixer.StopStreaming();
  }

  // tell the pin to go inactive and change state

  return CBaseFilter::Pause();
} // Pause

  //
  // Run
  //
  // Overriden to start the window streaming
  //
STDMETHODIMP COpenALFilter::Run(REFERENCE_TIME tStart)
{
  CAutoLock lock(this);
  HRESULT hr = NOERROR;
  FILTER_STATE fsOld = m_State;

  // This will call Pause if currently stopped

  hr = CBaseFilter::Run(tStart);
  if (FAILED(hr))
  {
    return hr;
  }

  if (fsOld != State_Running)
  {
    m_mixer.StartStreaming();
    m_openal_device->StartDevice();
  }

  return NOERROR;
} // Run

STDMETHODIMP COpenALFilter::SetSyncSource(IReferenceClock * pClock)
{
  return CBaseFilter::SetSyncSource(pClock);
}

//
// Constructor
//
CAudioInputPin::CAudioInputPin(COpenALFilter *pFilter,
  HRESULT *phr,
  LPCWSTR pPinName) :
  CBaseInputPin(NAME("Audio Input Pin"), pFilter, pFilter, phr, pPinName)
{
  m_pFilter = pFilter;
} // (Constructor)

  //
  // Destructor does nothing
  //
CAudioInputPin::~CAudioInputPin()
{
} // (Destructor)

  //
  // BreakConnect
  //
  // This is called when a connection or an attempted connection is terminated
  // and allows us to reset the connection media type to be invalid so that
  // we can always use that to determine whether we are connected or not. We
  // leave the format block alone as it will be reallocated if we get another
  // connection or alternatively be deleted if the filter is finally released
  //
HRESULT CAudioInputPin::BreakConnect()
{
  // Check we have a valid connection

  if (m_mt.IsValid() == FALSE)
  {
    // Don't return an error here, because it could lead to
    // ASSERT failures when rendering media files in GraphEdit.
    return S_FALSE;
  }

  m_pFilter->Stop();

  // Reset the CLSIDs of the connected media type

  m_mt.SetType(&GUID_NULL);
  m_mt.SetSubtype(&GUID_NULL);
  return CBaseInputPin::BreakConnect();
} // BreakConnect

HRESULT CAudioInputPin::CheckOpenALMediaType(const WAVEFORMATEX* wave_format)
{
  // Set frequency
  m_pFilter->m_openal_device->setFrequency(wave_format->nSamplesPerSec);

  // Get supported layout
  auto supported_bitness = m_pFilter->m_openal_device->getSupportedBitness();

  // Get supported bitness
  auto supported_layouts = m_pFilter->m_openal_device->getSupportedSpeakerLayout();
  bool valid_channel_layout = false;
  bool valid_sample_type = false;

  // Normalize channels
  COpenALStream::SpeakerLayout speaker_layout;
  switch (wave_format->nChannels)
  {
  case 1:
    speaker_layout = COpenALStream::SpeakerLayout::Mono;
    break;
  case 2:
    speaker_layout = COpenALStream::SpeakerLayout::Stereo;
    break;
  case 4:
    speaker_layout = COpenALStream::SpeakerLayout::Quad;
    break;
  case 6:
    speaker_layout = COpenALStream::SpeakerLayout::Surround6;
    break;
  case 8:
    speaker_layout = COpenALStream::SpeakerLayout::Surround8;
    break;
  }

  if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
    [speaker_layout](COpenALStream::SpeakerLayout layout) { return layout == speaker_layout; }))
  {
    m_pFilter->m_openal_device->setSpeakerLayout(speaker_layout);
    valid_channel_layout = true;
  }
  else
  {
    return S_FALSE;
  }

  // Normalize bitness
  COpenALStream::MediaBitness media_bitness;
  if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
  {
    auto format = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wave_format);
    if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
    {
      media_bitness = COpenALStream::MediaBitness::bitfloat;
    }
    else if (format->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
    {
      switch (format->Format.wBitsPerSample)
      {
      case 8:
        media_bitness = COpenALStream::MediaBitness::bit8;
        break;
      case 16:
        media_bitness = COpenALStream::MediaBitness::bit16;
        break;
      case 24:
        media_bitness = COpenALStream::MediaBitness::bit24;
        break;
      case 32:
        media_bitness = COpenALStream::MediaBitness::bit32;
        break;
      }
    }
  }
  else if (wave_format->wFormatTag == WAVE_FORMAT_PCM)
  {
    auto format = reinterpret_cast<const PCMWAVEFORMAT*>(wave_format);
    switch (format->wBitsPerSample)
    {
    case 8:
      media_bitness = COpenALStream::MediaBitness::bit8;
      break;
    case 16:
      media_bitness = COpenALStream::MediaBitness::bit16;
      break;
    case 24:
      media_bitness = COpenALStream::MediaBitness::bit24;
      break;
    case 32:
      media_bitness = COpenALStream::MediaBitness::bit32;
      break;
    }
  }

  if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
    [media_bitness](COpenALStream::MediaBitness bitness) { return bitness == media_bitness; }))
  {
    m_pFilter->m_openal_device->setBitness(media_bitness);
    valid_sample_type = true;
  }

  if (valid_channel_layout && valid_sample_type)
  {
    return S_OK;
  }

  return S_FALSE;
}

//
// CheckMediaType
//
// Check that we can support a given proposed type
//
HRESULT CAudioInputPin::CheckMediaType(const CMediaType *pmt)
{
  CheckPointer(pmt, E_POINTER);

  auto pwfx = reinterpret_cast<const WAVEFORMATEX*>(pmt->Format());

  if (pwfx == nullptr)
  {
    return S_FALSE;
  }

  // Reject non-PCM
  if (pmt->majortype != MEDIATYPE_Audio)
  {
    return S_FALSE;
  }

  if (pmt->formattype != FORMAT_WaveFormatEx)
  {
    return S_FALSE;
  }

  if (pwfx->wFormatTag != WAVE_FORMAT_PCM && pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE && pwfx->wFormatTag != WAVE_FORMAT_IEEE_FLOAT)
  {
    return S_FALSE;
  }

  m_pFilter->m_mixer.m_is_float = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
  if (m_pFilter->m_mixer.m_is_float == false)
  {
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      auto format = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pwfx);
      if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
      {
        m_pFilter->m_mixer.m_is_float = true;
      }
    }
  }

  // Check if our OpenAL driver supports it
  auto hr = CheckOpenALMediaType(pwfx);
  if (SUCCEEDED(hr))
  {
    return hr;
  }

  return S_FALSE;
} // CheckMediaType

  //
  // SetMediaType
  //
  // Actually set the format of the input pin
  //
HRESULT CAudioInputPin::SetMediaType(const CMediaType *pmt)
{
  CheckPointer(pmt, E_POINTER);
  CAutoLock lock(m_pFilter);

  // Pass the call up to my base class

  HRESULT hr = CBaseInputPin::SetMediaType(pmt);
  if (SUCCEEDED(hr))
  {
    auto pwf = reinterpret_cast<const WAVEFORMATEX*>(pmt->Format());

    m_pFilter->m_mixer.m_nChannels = pwf->nChannels;
    m_pFilter->m_mixer.m_nSamplesPerSec = pwf->nSamplesPerSec;
    m_pFilter->m_mixer.m_nBitsPerSample = pwf->wBitsPerSample;
    m_pFilter->m_mixer.m_nBlockAlign = pwf->nBlockAlign;
    m_pFilter->m_mixer.m_is_float = (pwf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

    auto hrr = CheckOpenALMediaType(pwf);
    if (SUCCEEDED(hrr))
    {
      return hrr;
    }
  }

  return hr;
} // SetMediaType

  //
  // Active
  //
  // Implements the remaining IMemInputPin virtual methods
  //
HRESULT CAudioInputPin::Active(void)
{
  return NOERROR;
} // Active

  //
  // Inactive
  //
  // Called when the filter is stopped
  //
HRESULT CAudioInputPin::Inactive(void)
{
  return NOERROR;
} // Inactive

  //
  // Receive
  //
  // Here's the next block of data from the stream
  //
HRESULT CAudioInputPin::Receive(IMediaSample * pSample)
{
  // Lock this with the filter-wide lock
  CAutoLock receive_lock(&m_receiveMutex);

  {
    CAutoLock object_lock(this);

    // If we're stopped, then reject this call
    // (the filter graph may be in mid-change)
    if (m_pFilter->m_State == State_Stopped)
    {
      return VFW_E_WRONG_STATE;
    }

    // Check all is well with the base class
    HRESULT hr = CBaseInputPin::Receive(pSample);
    if (FAILED(hr))
    {
      return hr;
    }

    if (m_SampleProps.dwSampleFlags & AM_SAMPLE_TYPECHANGED)
    {
      // TODO: don't recreate the device when possible
      //m_renderer.Finish(false, &m_bufferFilled);
      hr = SetMediaType(static_cast<CMediaType*>(m_SampleProps.pMediaType));
      if (FAILED(hr))
      {
        return hr;
      }

      // Clear queues
      m_pFilter->m_mixer.m_sample_queue.clear();
    }

    //if (m_eosUp)
    //  return S_FALSE;
  }

  // Send the sample to the video window object for rendering
  return m_pFilter->m_mixer.Receive(pSample);
} // Receive

STDMETHODIMP CAudioInputPin::EndOfStream()
{
  //m_pFilter->m_flush = true;

  m_pFilter->NotifyEvent(EC_COMPLETE, S_OK, (LONG_PTR)m_pFilter);
  return S_OK;
}

STDMETHODIMP CAudioInputPin::BeginFlush()
{
  // Parent method locks the object before modifying it, all is good.
  CBaseInputPin::BeginFlush();

  // Barrier for any present Receive() and EndOfStream() calls.
  // Subsequent ones will be rejected because m_bFlushing == TRUE.
  CAutoLock receiveLock(&m_receiveMutex);

  m_pFilter->m_mixer.m_sample_queue.clear();

  return S_OK;
}

STDMETHODIMP CAudioInputPin::EndFlush()
{
  // Parent method locks the object before modifying it, all is good.
  CBaseInputPin::EndFlush();

  return S_OK;
}

STDMETHODIMP CAudioInputPin::ReceiveCanBlock()
{
  return S_OK;
}

//
// CMixer Constructor
//
CMixer::CMixer(TCHAR *pName, COpenALFilter *pRenderer, HRESULT *phr) :
  m_hInstance(g_hInst),
  m_pRenderer(pRenderer),
  m_bStreaming(false),
  m_LastMediaSampleSize(0)
{
  ASSERT(m_pRenderer);
} // (Constructor)

  //
  // Destructor
  //
CMixer::~CMixer()
{
  // Ensure we stop streaming and release any samples

  StopStreaming();
} // (Destructor)

  //
  // StartStreaming
  //
  // This is called when we start running state
  //
HRESULT CMixer::StartStreaming()
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
HRESULT CMixer::StopStreaming()
{
  CAutoLock cAutoLock(this);

  // Have we been stopped already

  if (m_bStreaming == false)
  {
    return NOERROR;
  }

  m_bStreaming = false;
  return NOERROR;
} // StopStreaming

bool CMixer::IsStreaming()
{
  return m_bStreaming;
}

//
// CopyWaveformToBuffer
//
// Little endian?

void CMixer::CopyWaveform(IMediaSample *pMediaSample)
{
  //waitobject
  constexpr size_t bits_per_byte = 8;
  BYTE *pWave;                // Pointer to image data
  int  nBytes;

  ASSERT(pMediaSample);
  if (!pMediaSample)
    return;

  pMediaSample->GetPointer(&pWave);
  ASSERT(pWave != nullptr);

  nBytes = pMediaSample->GetActualDataLength();
  nBytes = nBytes - nBytes % (m_nChannels * m_nBitsPerSample / bits_per_byte);

  size_t pushed_bytes = 0;
  BYTE* pb = pWave;
  {
    uint8_t value = 0;
    for (int i = 0; i < nBytes; ++i)
    {
      value = *pb++;
      m_sample_queue.push(value);
      ++pushed_bytes;
    }
  }

  // Locking between inbound samples and the mixer
  m_rendered_samples = pushed_bytes / m_nChannels / m_nBitsPerSample * bits_per_byte;

  //std::ostringstream string;
  //string << "Size of the queue: " << pushed_bytes / m_nChannels / m_nBitsPerSample * bits_per_byte << " frames.\n";
  //OutputDebugStringA(string.str().c_str());

  m_samples_ready = true;
  m_samples_ready_cv.notify_one();

  {
    std::unique_lock<std::mutex> lk_rs(m_request_samples_mutex);
    while (m_request_samples == false)
    {
      // Wait for only 500 ms, just in case
      m_request_samples_cv.wait_for(lk_rs, std::chrono::milliseconds(500));
    }

    // We already delivered them, set it back to false
    m_request_samples = false;
  }
} // CopyWaveform

//
// Receive
//
// Called when the input pin receives another sample.
// Copy the waveform to our circular 1 second buffer
//
HRESULT CMixer::Receive(IMediaSample *pSample)
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
    CopyWaveform(pSample);     // Copy data to our circular buffer

    return NOERROR;
  }

  return NOERROR;
} // Receive

HRESULT CMixer::WaitForFrames(size_t num_of_bits)
{
  if (m_bStreaming)
  {
    std::unique_lock<std::mutex> lk(m_samples_ready_mutex);
    while (!m_samples_ready)
    {
      if (!m_bStreaming)
      {
        return E_FAIL;
      }

      // Check of bitness changed
      if (num_of_bits != m_nBitsPerSample)
      {
        return E_FAIL;
      }

      // Re-check everything after 30 ms
      m_samples_ready_cv.wait_for(lk, std::chrono::milliseconds(30));
    }

    // We already received them
    m_samples_ready = false;

    return S_OK;
  }
  else
  {
    return E_FAIL;
  }
}

size_t CMixer::Mix(std::vector<int8_t>* samples, size_t num_frames, size_t num_bytes_per_sample)
{
  if (!samples)
    return 0;

  // 2 = stereo
  // 6 = 5.1
  m_desired_bytes = num_frames * m_nChannels * num_bytes_per_sample;
  samples->resize(m_desired_bytes);

  // Wait for queue to fill
  size_t effective_samples = 0;

  // Still need to check EOS
  bool all_ok = false;
  while (m_sample_queue.unsafe_size() < m_desired_bytes) //&& m_notEOS)
  {
    m_request_samples = true;
    m_request_samples_cv.notify_one();

    constexpr size_t bits_per_byte = 8;
    if (WaitForFrames(num_bytes_per_sample * bits_per_byte) == S_OK)
    {
      all_ok = true;
    }
    else
    {
      break;
    }
  }

  for (size_t i = 0; i < m_desired_bytes; ++i)
  {
    int8_t value = 0;
    if (m_sample_queue.try_pop(value))
    {
      (*samples)[i] = value;
    }
  }
  // Set EOS samples here
  effective_samples = m_desired_bytes / num_bytes_per_sample;

  return effective_samples / m_nChannels;
}

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterServer
//
// Handles DLL registry
//
STDAPI DllRegisterServer()
{
  return AMovieDllRegisterServer2(TRUE);
} // DllRegisterServer

  //
  // DllUnregisterServer
  //
STDAPI DllUnregisterServer()
{
  return AMovieDllRegisterServer2(FALSE);
} // DllUnregisterServer

  //
  // DllEntryPoint
  //
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
  DWORD  dwReason,
  LPVOID lpReserved)
{
  return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}