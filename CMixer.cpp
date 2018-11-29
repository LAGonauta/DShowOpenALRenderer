#include "BaseHeader.h"

//
// CMixer Constructor
//
CMixer::CMixer(TCHAR *pName, HRESULT *phr) :
  m_hInstance(g_hInst),
  m_bStreaming(false),
  m_LastMediaSampleSize(0)
{
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