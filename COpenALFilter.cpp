#include "BaseHeader.h"

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
  m_mixer(new CMixer(NAME("OpenAL Renderer Mixer"), phr))
{
  ASSERT(phr);

  // Create the single input pin
  m_pInputPin = new CAudioInputPin(this, phr, L"Audio Input Pin");
  if (m_pInputPin == nullptr)
  {
    if (phr)
      *phr = E_OUTOFMEMORY;
  }

  m_openal_device = new COpenALStream(m_mixer, static_cast<IBaseFilter*>(this), phr);
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

//REFERENCE_TIME COpenALFilter::GetPrivateTime()
//{
//  return REFERENCE_TIME();
//}

HRESULT COpenALFilter::NonDelegatingQueryInterface(REFIID riid, void ** ppv)
{
  CheckPointer(ppv, E_POINTER);

  if (riid == IID_IUnknown)
  {
    return CUnknown::NonDelegatingQueryInterface(riid, ppv);
  }

  //if (riid == IID_IReferenceClock || riid == IID_IReferenceClockTimerControl)
  //{
  //  return GetInterface(static_cast<IReferenceClock*>(this), ppv);
  //}

  if (riid == IID_IBasicAudio)
  {
    return GetInterface(static_cast<IBasicAudio*>(m_openal_device), ppv);
  }

  if (riid == IID_IMediaSeeking)
  {
    if (m_seeking == nullptr)
    {
      // Actually should be true, but still not implemented
      HRESULT hr = CreatePosPassThru(CBaseFilter::GetOwner(), TRUE, m_pInputPin, &m_seeking);
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
    m_mixer->StopStreaming();
    m_openal_device->resetSampleTime();
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
    m_mixer->StartStreaming();
    m_openal_device->StartDevice();
  }

  return NOERROR;
} // Run

STDMETHODIMP COpenALFilter::SetSyncSource(IReferenceClock * pClock)
{
  return CBaseFilter::SetSyncSource(pClock);
}