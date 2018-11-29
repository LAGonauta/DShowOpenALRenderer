#pragma once

class CMixer : public CCritSec
{
  friend class CAudioInputPin;
  friend class COpenALFilter;

private:

  HINSTANCE m_hInstance;          // Global module instance handle

  std::atomic<bool> m_bStreaming; // Are we currently streaming

  int m_LastMediaSampleSize;      // Size of last MediaSample

  int m_nChannels;                // number of active channels
  int m_nSamplesPerSec;           // Samples per second
  int m_nBitsPerSample;           // Number bits per sample
  bool m_is_float;
  int m_nBlockAlign;              // Alignment on the samples
  size_t m_desired_bytes = 0;

  void CopyWaveform(IMediaSample *pMediaSample);
  HRESULT CMixer::WaitForFrames(size_t num_of_bits);

  concurrency::concurrent_queue<int8_t> m_sample_queue;

  // Locking between inbound samples and the mixer
  std::atomic<size_t> m_rendered_samples = 0;

  std::atomic<bool> m_samples_ready = false;
  std::mutex m_samples_ready_mutex;
  std::condition_variable m_samples_ready_cv;

  std::atomic<bool> m_request_samples = false;
  std::mutex m_request_samples_mutex;
  std::condition_variable m_request_samples_cv;

public:

  // Constructors and destructors

  CMixer(TCHAR *pName, HRESULT *phr);
  virtual ~CMixer();

  HRESULT StartStreaming();
  HRESULT StopStreaming();
  bool IsStreaming();

  // Called when the input pin receives a sample
  HRESULT Receive(IMediaSample* pIn);
  size_t Mix(std::vector<int8_t>* samples, size_t num_frames, size_t num_bytes_per_sample);
}; // CMixer