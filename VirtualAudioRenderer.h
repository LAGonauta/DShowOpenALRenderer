//------------------------------------------------------------------------------
// File: Scope.h
//
// Desc: DirectShow sample code - header file for audio oscilloscope filter.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//------------------------------------------------------------------------------

#include "OpenALStream.h"

// { 35919F40-E904-11ce-8A03-00AA006ECB65 }
DEFINE_GUID(CLSID_Scope,
  0x35919f40, 0xe904, 0x11ce, 0x8a, 0x3, 0x0, 0xaa, 0x0, 0x6e, 0xcb, 0x65);

class CScopeFilter;
class CScopeWindow;
class COpenALOutput;

// Class supporting the scope input pin

class CScopeInputPin : public CBaseInputPin, public CCritSec
{
  friend class CScopeFilter;
  friend class CScopeWindow;

private:

  CScopeFilter *m_pFilter;         // The filter that owns us

public:

  CScopeInputPin(CScopeFilter *pTextOutFilter,
    HRESULT *phr,
    LPCWSTR pPinName);
  ~CScopeInputPin();

  // Lets us know where a connection ends
  HRESULT BreakConnect();

  // Check that we can support this input type
  HRESULT CheckMediaType(const CMediaType *pmt);

  // Actually set the current format
  HRESULT SetMediaType(const CMediaType *pmt);

  // IMemInputPin virtual methods

  // Override so we can show and hide the window
  HRESULT Active(void);
  HRESULT Inactive(void);

  // Here's the next block of data from the stream.
  // AddRef it if you are going to hold onto it
  STDMETHODIMP Receive(IMediaSample *pSample);

  // Input lock mutex
  HANDLE m_input_mutex;

}; // CScopeInputPin


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

class CScopeWindow : public CCritSec
{
  friend class CScopeInputPin;
  friend class CScopeFilter;

private:

  HINSTANCE m_hInstance;          // Global module instance handle
  CScopeFilter *m_pRenderer;      // The owning renderer object

  BOOL m_bStreaming;              // Are we currently streaming

  int m_nPoints;                  // Size of m_pPoints[1|2]
  int m_nIndex;                   // Index of last sample written
  int m_LastMediaSampleSize;      // Size of last MediaSample

  int m_nChannels;                // number of active channels
  int m_nSamplesPerSec;           // Samples per second
  int m_nBitsPerSample;           // Number bits per sample
  int m_nBlockAlign;              // Alignment on the samples
  int m_MaxValue;                 // Max Value of the POINTS array

  void CopyWaveform(IMediaSample *pMediaSample);

public:

  // Constructors and destructors

  CScopeWindow(TCHAR *pName, CScopeFilter *pRenderer, HRESULT *phr);
  virtual ~CScopeWindow();

  HRESULT StartStreaming();
  HRESULT StopStreaming();

  // Called when the input pin receives a sample
  HRESULT Receive(IMediaSample * pIn);

}; // CScopeWindow

   // This is the COM object that represents the oscilloscope filter

class CScopeFilter : public CBaseFilter, public CCritSec
{

public:
  // Implements the IBaseFilter and IMediaFilter interfaces

  //  Make one of these
  //static CUnknown *CreateInstance(LPUNKNOWN punk, HRESULT *phr);

  //  Constructor
  CScopeFilter(LPUNKNOWN pUnk, HRESULT *phr);

  DECLARE_IUNKNOWN

  STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
  STDMETHODIMP Stop();
  STDMETHODIMP Pause();
  STDMETHODIMP Run(REFERENCE_TIME tStart);
  STDMETHODIMP SetSyncSource(IReferenceClock *pClock);

  // OpenAL
  COpenALStream* m_openal_device;

  // Add samples to OpenAL queue
  void PrintOpenALQueueBack();
  virtual ~CScopeFilter();

  // Return the pins that we support
  int GetPinCount();
  CBasePin *GetPin(int n);

  // This goes in the factory template table to create new instances
  static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

  STDMETHODIMP JoinFilterGraph(IFilterGraph * pGraph, LPCWSTR pName);

private:

  // The nested classes may access our private state
  friend class CScopeInputPin;
  friend class CScopeWindow;
  friend class COpenALOutput;

  CScopeInputPin *m_pInputPin;   // Handles pin interfaces
  CScopeWindow m_Window;         // Looks after the window
  //CCritSec m_Lock;     // Locking

}; // CScopeFilter