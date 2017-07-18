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
  CBaseFilter(NAME("OpenAL Renderer"), pUnk, (CCritSec *) this, CLSID_OALRend),
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
      HRESULT hr = CreatePosPassThru(GetOwner(), FALSE, m_pInputPin, &m_seeking);
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

  switch (wave_format->nChannels)
  {
  case 1:
  {
    if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
      [](COpenALStream::SpeakerLayout layout) { return layout == COpenALStream::SpeakerLayout::Mono; }))
    {
      m_pFilter->m_openal_device->setSpeakerLayout(COpenALStream::SpeakerLayout::Mono);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 2:
  {
    if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
      [](COpenALStream::SpeakerLayout layout) { return layout == COpenALStream::SpeakerLayout::Stereo; }))
    {
      m_pFilter->m_openal_device->setSpeakerLayout(COpenALStream::SpeakerLayout::Stereo);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 4:
  {
    if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
      [](COpenALStream::SpeakerLayout layout) { return layout == COpenALStream::SpeakerLayout::Quad; }))
    {
      m_pFilter->m_openal_device->setSpeakerLayout(COpenALStream::SpeakerLayout::Quad);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 6:
  {
    if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
      [](COpenALStream::SpeakerLayout layout) { return layout == COpenALStream::SpeakerLayout::Surround6; }))
    {
      m_pFilter->m_openal_device->setSpeakerLayout(COpenALStream::SpeakerLayout::Surround6);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 8:
  {
    if (std::any_of(supported_layouts.cbegin(), supported_layouts.cend(),
      [](COpenALStream::SpeakerLayout layout) { return layout == COpenALStream::SpeakerLayout::Surround8; }))
    {
      m_pFilter->m_openal_device->setSpeakerLayout(COpenALStream::SpeakerLayout::Surround8);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  }

  switch (wave_format->wBitsPerSample)
  {
  case 8:
  {
    if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
      [](COpenALStream::MediaBitness bitness) { return bitness == COpenALStream::MediaBitness::bit8; }))
    {
      m_pFilter->m_openal_device->setBitness(COpenALStream::MediaBitness::bit8);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 16:
  {
    if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
      [](COpenALStream::MediaBitness bitness) { return bitness == COpenALStream::MediaBitness::bit16; }))
    {
      m_pFilter->m_openal_device->setBitness(COpenALStream::MediaBitness::bit16);
    }
    else
    {
      return S_FALSE;
    }
    break;
  }
  case 32:
  {
    if (wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
    {
      if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
        [](COpenALStream::MediaBitness bitness) { return bitness == COpenALStream::MediaBitness::bitfloat; }))
      {
        m_pFilter->m_openal_device->setBitness(COpenALStream::MediaBitness::bitfloat);
      }
      else
      {
        return S_FALSE;
      }
    }
    else
    {
      if (std::any_of(supported_bitness.cbegin(), supported_bitness.cend(),
        [](COpenALStream::MediaBitness bitness) { return bitness == COpenALStream::MediaBitness::bit32; }))
      {
        m_pFilter->m_openal_device->setBitness(COpenALStream::MediaBitness::bit32);
      }
      else
      {
        return S_FALSE;
      }
    }
    break;
  }
  }

  return S_OK;
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

  // Reject non-PCM or float Audio type
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

  // Check if our OpenAL driver supports it
  auto hr = CheckOpenALMediaType(pwfx);
  if (FAILED(hr))
  {
    return hr;
  }

  return S_OK;

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
    if (FAILED(hrr))
    {
      //return hrr;
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
      m_pFilter->m_mixer.m_sample_queue_8bit.clear();
      m_pFilter->m_mixer.m_sample_queue_32bit.clear();
      m_pFilter->m_mixer.m_sample_queue_float.clear();
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
// CopyWaveform
//
// Copy the current MediaSample into a POINT array so we can use GDI
// to paint the waveform.  The POINT array contains a 1 second history
// of the past waveform.  The "Y" values are normalized to a range of
// +128 to -127 within the POINT array.
//
void CMixer::CopyWaveform(IMediaSample *pMediaSample)
{
  //waitobject
  BYTE *pWave;                // Pointer to image data
  int  nBytes;
  int  nSamplesPerChan;

  ASSERT(pMediaSample);
  if (!pMediaSample)
    return;

  pMediaSample->GetPointer(&pWave);
  ASSERT(pWave != nullptr);

  nBytes = pMediaSample->GetActualDataLength();
  nSamplesPerChan = nBytes / m_nBlockAlign;

  size_t pushed_samples = 0;
  if (m_nBitsPerSample == 8)
  {
    BYTE* pb = pWave;

    while (nSamplesPerChan--)
    {
      uint8_t value = 0;
      for (int i = 0; i < m_nChannels; ++i)
      {
        value = *pb++;
        m_sample_queue_8bit.push(value);
        ++pushed_samples;
      }
    }
  }
  else if (m_nBitsPerSample == 16)
  {
    WORD* pw = (WORD*)pWave;

    while (nSamplesPerChan--)
    {
      uint16_t value = 0;
      for (int i = 0; i < m_nChannels; ++i)
      {
        value = *pw++;
        m_sample_queue.push(value);
        ++pushed_samples;
      }
    }
  }
  else if (m_nBitsPerSample == 32)
  {
    if (m_is_float)
    {
      float_t* pdw = (float_t*)pWave;

      while (nSamplesPerChan--)
      {
        float_t value = 0;
        for (int i = 0; i < m_nChannels; ++i)
        {
          value = *pdw++;
          m_sample_queue_float.push(value);
          ++pushed_samples;
        }
      }
    }
    else
    {
      DWORD* pdw = (DWORD*)pWave;

      while (nSamplesPerChan--)
      {
        uint32_t value = 0;
        for (int i = 0; i < m_nChannels; ++i)
        {
          value = *pdw++;
          m_sample_queue_32bit.push(value);
          ++pushed_samples;
        }
      }
    }
  }

  // Locking between inbound samples and the mixer
  m_rendered_samples = pushed_samples;

  //std::ostringstream string;
  //string << "Size of the queue: " << pushed_samples / m_nChannels << " frames.\n";
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

size_t CMixer::Mix(std::vector<int8_t>* samples, size_t num_frames)
{
  if (!samples)
    return 0;

  // 2 = stereo
  // 6 = 5.1
  m_desired_samples = num_frames * m_nChannels;
  samples->resize(m_desired_samples);

  // Wait for queue to fill
  size_t effective_samples = 0;

  // Still need to check EOS
  bool all_ok = false;
  while (m_sample_queue_8bit.unsafe_size() < m_desired_samples) //&& m_notEOS)
  {
    m_request_samples = true;
    m_request_samples_cv.notify_one();

    if (WaitForFrames(8) == S_OK)
    {
      all_ok = true;
    }
    else
    {
      break;
    }
  }

  for (size_t i = 0; i < m_desired_samples; ++i)
  {
    int8_t value = 0;
    if (m_sample_queue_8bit.try_pop(value))
    {
      (*samples)[i] = value;
    }
  }
  // Set EOS samples here
  effective_samples = m_desired_samples;

  return effective_samples / m_nChannels;
}

size_t CMixer::Mix(std::vector<int16_t>* samples, size_t num_frames)
{
  if (!samples)
    return 0;

  // 2 = stereo
  // 6 = 5.1
  m_desired_samples = num_frames * m_nChannels;
  samples->resize(m_desired_samples);

  // Wait for queue to fill
  size_t effective_samples = 0;

  // Still need to check EOS
  bool all_ok = false;
  while (m_sample_queue.unsafe_size() < m_desired_samples) //&& m_notEOS)
  {
    m_request_samples = true;
    m_request_samples_cv.notify_one();

    if (WaitForFrames(16) == S_OK)
    {
      all_ok = true;
    }
    else
    {
      break;
    }
  }

  for (size_t i = 0; i < m_desired_samples; ++i)
  {
    int16_t value = 0;
    if (m_sample_queue.try_pop(value))
    {
      (*samples)[i] = value;
    }
  }
  // Set EOS samples here
  effective_samples = m_desired_samples;

  return effective_samples / m_nChannels;
}

size_t CMixer::Mix(std::vector<int32_t>* samples, size_t num_frames)
{
  if (!samples)
    return 0;

  // 2 = stereo
  // 6 = 5.1
  m_desired_samples = num_frames * m_nChannels;
  samples->resize(m_desired_samples);

  // Wait for queue to fill
  size_t effective_samples = 0;

  // Still need to check EOS
  bool all_ok = false;
  while (m_sample_queue_32bit.unsafe_size() < m_desired_samples) //&& m_notEOS)
  {
    m_request_samples = true;
    m_request_samples_cv.notify_one();

    if (WaitForFrames(32) == S_OK)
    {
      all_ok = true;
    }
    else
    {
      break;
    }
  }

  for (size_t i = 0; i < m_desired_samples; ++i)
  {
    int32_t value = 0;
    if (m_sample_queue_32bit.try_pop(value))
    {
      (*samples)[i] = value;
    }
  }

  // Set EOS samples here
  effective_samples = m_desired_samples;

  return effective_samples / m_nChannels;
}

size_t CMixer::Mix(std::vector<float_t>* samples, size_t num_frames)
{
  if (!samples)
    return 0;

  // 2 = stereo
  // 6 = 5.1
  m_desired_samples = num_frames * m_nChannels;
  samples->resize(m_desired_samples);

  // Wait for queue to fill
  size_t effective_samples = 0;

  // Still need to check EOS
  bool all_ok = false;
  while (m_sample_queue_float.unsafe_size() < m_desired_samples) //&& m_notEOS)
  {
    m_request_samples = true;
    m_request_samples_cv.notify_one();

    if (WaitForFrames(32) == S_OK)
    {
      all_ok = true;
    }
    else
    {
      break;
    }
  }

  for (size_t i = 0; i < m_desired_samples; ++i)
  {
    float_t value = 0;
    if (m_sample_queue_float.try_pop(value))
    {
      (*samples)[i] = value;
    }
  }

  // Set EOS samples here
  effective_samples = m_desired_samples;

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

