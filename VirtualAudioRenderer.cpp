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


//
// Summary
//
// This is an audio oscilloscope renderer - we are basically an audio renderer.
// When we are created we also create a class to handle the scope window
// whose constructor creates a worker thread; when it is destroyed it will
// also terminate the worker thread. On that worker thread a window is handled
// that shows the audio waveform for data sent to us. The data is kept
// in a circular buffer that loops when sufficient data has been received.
// We support a number of different audio formats such as 8-bit mode and stereo.
//
//
// Demonstration Instructions
//
// (To really sure of this demonstration the machine must have a sound card)
//
// Start GraphEdit, which is available in the SDK DXUtils folder. Drag and drop
// an MPEG, AVI or MOV file into the tool and it will be rendered. Then go to
// the filters in the graph and find the filter (box) titled "Audio Renderer"
// This is the filter we will be replacing with this oscilloscope renderer.
// Then click on the box and hit DELETE. After that go to the Graph menu and
// select "Insert Filters", from the dialog box that pops up find and select
// "Oscilloscope", then dismiss the dialog. Back in the graph layout find the
// output pin of the filter that was connected to the input of the audio
// renderer you just deleted, right click and select "Render". You should
// see it being connected to the input pin of the oscilloscope you inserted
//
// Click Run on GraphEdit and you'll see a waveform for the audio soundtrack...
//
//
// Files
//
// resource.h           Microsoft Visual C++ generated file
// scope.cpp            The main filter and window implementations
// scope.def            What APIs the DLL imports and exports
// scope.h              Window and filter class definitions
// scope.mak            Visual C++ generated makefile
// scope.rc             Dialog box template for our window
//
//
// Base classes we use
//
// CBaseInputPin        A generic input pin we use for the filter
// CCritSec             A wrapper class around a critical section
// CBaseFilter          The generic DirectShow filter object
//
//

#include <streams.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <initguid.h>
#include <wxdebug.h>
#include "VirtualAudioRenderer.h"
#include "OpenALStream.h"
#include <strsafe.h>

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
  L"Output",                  // Connects to pin
  1,                          // Number of pins types
  &sudPinTypes };            // Pin information


const AMOVIESETUP_FILTER sudScope =
{
  &CLSID_Scope,               // Filter CLSID
  L"Oscilloscope",            // String name
  MERIT_DO_NOT_USE,           // Filter merit
  1,                          // Number pins
  &sudPins                    // Pin details
};


// List of class IDs and creator functions for class factory

CFactoryTemplate g_Templates[] = {
  { L"Oscilloscope"
  , &CLSID_Scope
  , (LPFNNewCOMObject)COpenALFilter::CreateInstance
  , NULL
  , &sudScope }
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

void COpenALFilter::PrintOpenALQueueBack()
{
  //static unsigned int number = 0;
  //if (number >= 24000)
  //{
  //  auto value = m_openal_device.m_audio_buffer_queue.unsafe_begin();
  //  wchar_t string_buf[1024] = { 0 };
  //  swprintf(string_buf, L"Back value: %d\n", &value);
  //  OutputDebugString(string_buf);
  //  number = 0;
  //}
  //else
  //{
  //  ++number;
  //}
}

COpenALFilter::COpenALFilter(LPUNKNOWN pUnk, HRESULT *phr) :
  CBaseFilter(NAME("Oscilloscope"), pUnk, (CCritSec *) this, CLSID_Scope),
  m_Window(NAME("Oscilloscope"), this, phr)
{
  ASSERT(phr);

  // Create the single input pin
  m_pInputPin = new CAudioInputPin(this, phr, L"Scope Input Pin");
  if (m_pInputPin == NULL)
  {
    if (phr)
      *phr = E_OUTOFMEMORY;
  }

  m_openal_device = new COpenALStream(m_pInputPin, static_cast<IBaseFilter*>(this), phr);

} // (Constructor)


  //
  // Destructor
  //
COpenALFilter::~COpenALFilter()
{
  // Delete the contained interfaces

  ASSERT(m_pInputPin);
  delete m_pInputPin;
  m_pInputPin = NULL;

  ASSERT(m_openal_device);
  delete m_openal_device;
  m_openal_device = NULL;

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
    return NULL;
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

  if (riid == IID_IReferenceClock)
  {
    return GetInterface(static_cast<IReferenceClock*>(m_openal_device), ppv);
  }
  else
  {
    return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
  }
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
    m_Window.StopStreaming();
  }

  // tell the pin to go inactive and change state

  if (m_State == State_Stopped)
  {
    m_openal_device->StartDevice();
  }

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
    m_Window.StartStreaming();
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
  CBaseInputPin(NAME("Scope Input Pin"), pFilter, pFilter, phr, pPinName)
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


  //
  // CheckMediaType
  //
  // Check that we can support a given proposed type
  //
HRESULT CAudioInputPin::CheckMediaType(const CMediaType *pmt)
{
  CheckPointer(pmt, E_POINTER);

  WAVEFORMATEX *pwfx = (WAVEFORMATEX *)pmt->Format();

  if (pwfx == NULL)
    return E_INVALIDARG;

  // Reject non-PCM Audio type

  if (pmt->majortype != MEDIATYPE_Audio)
  {
    return E_INVALIDARG;
  }

  if (pmt->formattype != FORMAT_WaveFormatEx)
  {
    return E_INVALIDARG;
  }

  if (pwfx->wFormatTag != WAVE_FORMAT_PCM && pwfx->wFormatTag != WAVE_FORMAT_EXTENSIBLE)
  {
    return E_INVALIDARG;
  }

  return NOERROR;

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
    WAVEFORMATEX *pwf = (WAVEFORMATEX *)pmt->Format();

    m_pFilter->m_Window.m_nChannels = pwf->nChannels;
    m_pFilter->m_Window.m_nSamplesPerSec = pwf->nSamplesPerSec;
    m_pFilter->m_Window.m_nBitsPerSample = pwf->wBitsPerSample;
    m_pFilter->m_Window.m_nBlockAlign = pwf->nBlockAlign;

    m_pFilter->m_Window.m_MaxValue = 128;
    m_pFilter->m_Window.m_nIndex = 0;

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
  CAutoLock lock(m_pFilter);

  // If we're stopped, then reject this call
  // (the filter graph may be in mid-change)
  if (m_pFilter->m_State == State_Stopped)
  {
    return E_FAIL;
  }

  // Check all is well with the base class
  HRESULT hr = CBaseInputPin::Receive(pSample);
  if (FAILED(hr))
  {
    return hr;
  }

  // Send the sample to the video window object for rendering
  return m_pFilter->m_Window.Receive(pSample);

} // Receive

  //
  // CMixer Constructor
  //
CMixer::CMixer(TCHAR *pName, COpenALFilter *pRenderer, HRESULT *phr) :
  m_hInstance(g_hInst),
  m_pRenderer(pRenderer),
  m_nPoints(0),
  m_bStreaming(FALSE),
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

  if (m_bStreaming == TRUE)
  {
    return NOERROR;
  }

  m_bStreaming = TRUE;
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

  if (m_bStreaming == FALSE)
  {
    return NOERROR;
  }

  m_bStreaming = FALSE;
  return NOERROR;

} // StopStreaming

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
  BYTE *pWave;                // Pointer to image data
  int  nBytes;
  int  nSamplesPerChan;

  ASSERT(pMediaSample);
  if (!pMediaSample)
    return;

  pMediaSample->GetPointer(&pWave);
  ASSERT(pWave != NULL);

  nBytes = pMediaSample->GetActualDataLength();
  nSamplesPerChan = nBytes / m_nBlockAlign;

  m_pRenderer->m_openal_device->m_frequency = m_nSamplesPerSec;
  switch (m_nBitsPerSample + m_nChannels)
  {
    BYTE * pb;
    WORD * pw;

  case 9:
  {   // Mono, 8-bit
    m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit8;
    m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Mono;

    pb = pWave;
    while (nSamplesPerChan--)
    {
      uint8_t value = *pb++;  // Make zero centered
      m_pRenderer->m_openal_device->m_audio_buffer_queue_int8.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }

  case 10:
  {   // Stereo, 8-bit
    pb = pWave;
    while (nSamplesPerChan--)
    {
      m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit8;
      m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Stereo;

      uint8_t value = *pb++;  // Make zero centered
      m_pRenderer->m_openal_device->m_audio_buffer_queue_int8.push(value);

      value = *pb++;  // Make zero centered
      m_pRenderer->m_openal_device->m_audio_buffer_queue_int8.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }

  case 17:
  { // Mono, 16-bit
    m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit16;
    m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Mono;

    pw = (WORD *)pWave;
    while (nSamplesPerChan--)
    {
      int16_t value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }

  case 18:
  { // Stereo, 16-bit
    m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit16;
    m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Stereo;

    pw = (WORD *)pWave;
    while (nSamplesPerChan--)
    {
      int16_t value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }
  case 22:
  { // 5.1 Surround, 16-bit
    m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit16;
    m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Surround6;

    pw = (WORD *)pWave;
    while (nSamplesPerChan--)
    {
      int16_t value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }
  case 24:
  { // 7.1 Surround, 16-bit
    m_pRenderer->m_openal_device->m_media_type = m_pRenderer->m_openal_device->bit16;
    m_pRenderer->m_openal_device->m_speaker_layout = m_pRenderer->m_openal_device->Surround8;

    pw = (WORD *)pWave;
    while (nSamplesPerChan--)
    {
      int16_t value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      value = (short)*pw++;
      m_pRenderer->m_openal_device->m_audio_buffer_queue.push(value);

      if (++m_nIndex == m_nSamplesPerSec)
        m_nIndex = 0;
    }
    break;
  }

  default:
    ASSERT(0);
    break;

  } // End of format switch

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
  ASSERT(pSample != NULL);

  REFERENCE_TIME tStart, tStop;
  pSample->GetTime(&tStart, &tStop);

  // Ignore zero-length samples
  if ((m_LastMediaSampleSize = pSample->GetActualDataLength()) == 0)
    return NOERROR;

  if (m_bStreaming == TRUE)
  {
    CopyWaveform(pSample);     // Copy data to our circular buffer

    return NOERROR;
  }

  return NOERROR;

} // Receive


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

