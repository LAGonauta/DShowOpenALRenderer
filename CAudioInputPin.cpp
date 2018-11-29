#include <algorithm>
#include "BaseHeader.h"

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

  m_pFilter->m_mixer->m_is_float = (pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
  if (m_pFilter->m_mixer->m_is_float == false)
  {
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
    {
      auto format = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(pwfx);
      if (format->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
      {
        m_pFilter->m_mixer->m_is_float = true;
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

    m_pFilter->m_mixer->m_nChannels = pwf->nChannels;
    m_pFilter->m_mixer->m_nSamplesPerSec = pwf->nSamplesPerSec;
    m_pFilter->m_mixer->m_nBitsPerSample = pwf->wBitsPerSample;
    m_pFilter->m_mixer->m_nBlockAlign = pwf->nBlockAlign;
    m_pFilter->m_mixer->m_is_float = (pwf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

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
      m_pFilter->m_mixer->m_sample_queue.clear();
    }

    //if (m_eosUp)
    //  return S_FALSE;
  }

  // Send the sample to the video window object for rendering
  return m_pFilter->m_mixer->Receive(pSample);
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

  m_pFilter->m_mixer->m_sample_queue.clear();

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