#include <algorithm>
#include "BaseHeader.h"

//
// Constructor
//
CAudioInputPin::CAudioInputPin(COpenALFilter *pFilter, HRESULT *phr, LPCWSTR pPinName) :
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

  // Check if our OpenAL driver supports it
  auto hr = m_pFilter->m_openal_device->checkMediaType(pwfx);
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
  auto pwfx = reinterpret_cast<const WAVEFORMATEX*>(pmt->Format());
  if (pwfx == nullptr)
  {
    return S_FALSE;
  }
  auto hr = m_pFilter->m_openal_device->checkMediaType(pwfx);
  if (SUCCEEDED(hr))
  {
    hr = m_pFilter->m_openal_device->setMediaType(pmt);
    if (SUCCEEDED(hr))
    {
      hr = CBaseInputPin::SetMediaType(pmt);
      if (SUCCEEDED(hr))
      {
        return hr;
      }
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
    }

    //if (m_eosUp)
    //  return S_FALSE;
  }

  // Send the sample to the video window object for rendering
  return m_pFilter->m_openal_device->Receive(pSample);
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

  m_pFilter->m_openal_device->ResetBuffer();

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