//------------------------------------------------------------------------------
// File: Scope.h
//
// Desc: DirectShow sample code - header file for audio oscilloscope filter.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------

#include <concurrent_queue.h>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <comdef.h>

#include "OpenALStream.h"

// { 35919F40-E904-11ce-8A03-00AA006ECB65 }
DEFINE_GUID(CLSID_OALRend,
  0x35919f40, 0xe904, 0x11ce, 0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65);

class COpenALFilter;
class CMixer;
class COpenALOutput;

// Class supporting the scope input pin

class CAudioInputPin : public CCritSec, public CBaseInputPin
{
  friend class COpenALFilter;
  friend class CMixer;

private:

  COpenALFilter *m_pFilter;         // The filter that owns us
  CCritSec m_receiveMutex;

public:

  CAudioInputPin(COpenALFilter *pTextOutFilter,
    HRESULT *phr,
    LPCWSTR pPinName);
  ~CAudioInputPin();

  // Lets us know where a connection ends
  HRESULT BreakConnect();

  // Check that we can support this input type
  HRESULT CheckMediaType(const CMediaType *pmt) override;

  // Actually set the current format
  HRESULT SetMediaType(const CMediaType *pmt) override;

  // IMemInputPin virtual methods

  // Override so we can show and hide the window
  HRESULT Active(void) override;
  HRESULT Inactive(void) override;

  // Here's the next block of data from the stream.
  // AddRef it if you are going to hold onto it
  STDMETHODIMP Receive(IMediaSample *pSample) override;
  STDMETHODIMP ReceiveCanBlock() override;

}; // CAudioInputPin


   // This class looks after the management of a window. When the class gets
   // instantiated the constructor spawns off a worker thread that does all
   // the window work. The original thread waits until it is signaled to
   // continue. The worker thread first registers the window class if it
   // is not already done. Then it creates a window and sets it's size to
   // a default iWidth by iHeight dimensions. The worker thread MUST be the
   // one who creates the window as it is the one who calls GetMessage. When
   // it has done all this it signals the original thread which lets it
   // continue, this ensures a window is created and valid before the
   // constructor returns. The thread start address is the WindowMessageLoop
   // function. This takes as it's initialisation parameter a pointer to the
   // CVideoWindow object that created it, the function also initialises it's
   // window related member variables such as the handle and device contexts

   // These are the video window styles

class CMixer : public CCritSec
{
  friend class CAudioInputPin;
  friend class COpenALFilter;

private:

  HINSTANCE m_hInstance;          // Global module instance handle
  COpenALFilter *m_pRenderer;      // The owning renderer object

  std::atomic<BOOL> m_bStreaming;              // Are we currently streaming

  int m_LastMediaSampleSize;      // Size of last MediaSample

  int m_nChannels;                // number of active channels
  int m_nSamplesPerSec;           // Samples per second
  int m_nBitsPerSample;           // Number bits per sample
  int m_nBlockAlign;              // Alignment on the samples
  size_t m_desired_samples = 0;

  void CopyWaveform(IMediaSample *pMediaSample);
  HRESULT CMixer::WaitForFrames(size_t num_of_bits);

  concurrency::concurrent_queue<int16_t> m_sample_queue;
  concurrency::concurrent_queue<int8_t> m_sample_queue_8bit;
  concurrency::concurrent_queue<int32_t> m_sample_queue_32bit;

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
  size_t Mix(std::vector<int16_t>* samples, size_t num_frames);
  size_t Mix(std::vector<int32_t>* samples, size_t num_frames);

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

  // Add samples to OpenAL queue
  void PrintOpenALQueueBack();
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
  CMixer m_mixer;         // Looks after the window
  IUnknownPtr m_seeking;

}; // COpenALFilter