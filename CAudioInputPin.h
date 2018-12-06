#pragma once

class CAudioInputPin : public CCritSec, public CBaseInputPin
{
  friend class COpenALFilter;
  friend class CMixer;
private:
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