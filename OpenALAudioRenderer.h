//------------------------------------------------------------------------------
// File: Scope.h
//
// Desc: DirectShow sample code - header file for audio oscilloscope filter.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------

#include <concurrent_queue.h>
#include <vector>
#include <condition_variable>
#include <comdef.h>

#include "OpenALStream.h"

// {25B8D696-1510-49BF-A0C3-E38FAFD54782}
DEFINE_GUID(CLSID_OALRend,
  0x25b8d696, 0x1510, 0x49bf, 0xa0, 0xc3, 0xe3, 0x8f, 0xaf, 0xd5, 0x47, 0x82);

class COpenALFilter;
class CMixer;
class COpenALOutput;

class CAudioInputPin : public CCritSec, public CBaseInputPin
{
  friend class COpenALFilter;
  friend class CMixer;

private:

  HRESULT CheckOpenALMediaType(const WAVEFORMATEX* wave_format);
  COpenALFilter *m_pFilter;         // The filter that owns us
  CCritSec m_receiveMutex;

  std::atomic<bool> m_startEOS = false;
  std::atomic<bool> m_stopEOS = false;

public:

  CAudioInputPin(COpenALFilter *pTextOutFilter,
    HRESULT *phr,
    LPCWSTR pPinName);
  ~CAudioInputPin();

  // Lets us know where a connection ends
  HRESULT BreakConnect();

  // Check that we can support this input type
  HRESULT CheckMediaType(const CMediaType* pmt) override;

  // Actually set the current format
  HRESULT SetMediaType(const CMediaType* pmt) override;

  // IMemInputPin virtual methods

  // Override so we can show and hide the window
  HRESULT Active(void) override;
  HRESULT Inactive(void) override;

  // Here's the next block of data from the stream.
  // AddRef it if you are going to hold onto it
  STDMETHODIMP Receive(IMediaSample* pSample) override;
  STDMETHODIMP EndOfStream() override;
  STDMETHODIMP ReceiveCanBlock() override;
  STDMETHODIMP BeginFlush() override;
  STDMETHODIMP EndFlush() override;

}; // CAudioInputPin

class CMixer : public CCritSec
{
  friend class CAudioInputPin;
  friend class COpenALFilter;

private:

  HINSTANCE m_hInstance;          // Global module instance handle
  COpenALFilter* m_pRenderer;     // The owning renderer object

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

  CMixer(TCHAR *pName, COpenALFilter *pRenderer, HRESULT *phr);
  virtual ~CMixer();

  HRESULT StartStreaming();
  HRESULT StopStreaming();
  bool IsStreaming();

  // Called when the input pin receives a sample
  HRESULT Receive(IMediaSample* pIn);
  size_t Mix(std::vector<int8_t>* samples, size_t num_frames, size_t num_bytes_per_sample);
}; // CMixer

   // This is the COM object that represents the oscilloscope filter

class COpenALFilter : public CBaseFilter, public CCritSec
{
public:
  // Implements the IBaseFilter and IMediaFilter interfaces

  //  Make one of these
  //static CUnknown *CreateInstance(LPUNKNOWN punk, HRESULT *phr);

  //  Constructor
  COpenALFilter(LPUNKNOWN pUnk, HRESULT *phr);

  DECLARE_IUNKNOWN

  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv) override;
  STDMETHODIMP Stop() override;
  STDMETHODIMP Pause() override;
  STDMETHODIMP Run(REFERENCE_TIME tStart) override;
  STDMETHODIMP SetSyncSource(IReferenceClock *pClock) override;

  // OpenAL
  COpenALStream* m_openal_device;
  virtual ~COpenALFilter();

  // Return the pins that we support
  int GetPinCount();
  CBasePin *GetPin(int n);

  // This goes in the factory template table to create new instances
  static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

  STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);

private:

  // The nested classes may access our private state
  friend class CAudioInputPin;
  friend class CMixer;
  friend class COpenALOutput;

  CAudioInputPin *m_pInputPin;   // Handles pin interfaces
  CMixer m_mixer;                // Looks after the window
  IUnknownPtr m_seeking;

}; // COpenALFilter