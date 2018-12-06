#pragma once

class COpenALFilter : public CBaseFilter, public CCritSec
{
  friend class CAudioInputPin;
  friend class COpenALStream;
  friend class CMixer;

public:
  // Implements the IBaseFilter and IMediaFilter interfaces

  //  Make one of these
  //static CUnknown *CreateInstance(LPUNKNOWN punk, HRESULT *phr);

  //  Constructor
  COpenALFilter(LPUNKNOWN pUnk, HRESULT *phr);

  DECLARE_IUNKNOWN

  //REFERENCE_TIME GetPrivateTime() override;

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
  CAudioInputPin *m_pInputPin = nullptr;   // Handles pin interfaces
  IUnknownPtr m_seeking = nullptr;
}; // COpenALFilter