// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include <include/OpenAL/al.h>
#include <include/OpenAL/alc.h>

#ifndef __STREAMS__
#include "streams.h"
#endif

// OpenAL requires a minimum of two buffers, three or more recommended
const size_t OAL_BUFFERS = 8;

class CMixer;

class COpenALStream final : public CBaseReferenceClock, public CBasicAudio
{
  friend class CMixer;

public:
  enum SpeakerLayout
  {
    Mono,
    Stereo,
    Quad,
    Surround6,
    Surround8
  };

  enum MediaBitness
  {
    bit8,
    bit16,
    bit24,
    bit32,
    bitfloat
  };

  ~COpenALStream();
  COpenALStream(CMixer* audioMixer, LPUNKNOWN pUnk, HRESULT *phr);

  // We must make this time depend on the sound card buffers latter,
  // not on the system clock
  //REFERENCE_TIME GetPrivateTime() override;
  //HRESULT SetTimeDelta(const REFERENCE_TIME &TimeDelta);

  void SetSyncSource(IReferenceClock *pClock);
  void ClockController(HDRVR hdrvr, DWORD_PTR dwUser,
    DWORD_PTR dw2);

  IUnknown * pUnk()
  {
    return static_cast<IUnknown*>(static_cast<IReferenceClock*>(this));
  }

  STDMETHODIMP OpenDevice();
  STDMETHODIMP CloseDevice();
  STDMETHODIMP StartDevice();
  STDMETHODIMP StopDevice();

  STDMETHODIMP put_Volume(long volume) override;
  STDMETHODIMP get_Volume(long* pVolume) override;
  STDMETHODIMP put_Balance(long balance) override;
  STDMETHODIMP get_Balance(long* pBalance) override;
  long m_fake_balance = 0;

  HRESULT Stop();
  HRESULT setSpeakerLayout(SpeakerLayout layout);
  SpeakerLayout getSpeakerLayout();
  HRESULT setFrequency(uint32_t frequency);
  uint32_t getFrequency();
  HRESULT setBitness(MediaBitness bitness);
  MediaBitness getBitness();
  std::vector<MediaBitness> getSupportedBitness();
  std::vector<SpeakerLayout> getSupportedSpeakerLayout();
  // In milliseconds
  REFERENCE_TIME getSampleTime();
  HRESULT resetSampleTime();

private:
  STDMETHODIMP isValid();

  std::thread m_thread;
  std::atomic<bool> m_run_thread = false;

  void SoundLoop();
  void SetVolume(int volume);
  void Destroy();
  ALenum CheckALError(std::string desc);

  uint32_t num_buffers = OAL_BUFFERS;
  uint32_t num_buffers_queued = 0;

  std::vector<ALuint> m_buffers;
  std::atomic<size_t> m_total_buffered = 0;
  ALuint m_source = 0;
  std::atomic<ALfloat> m_volume = 1.0f;

  CMixer* m_mixer;
  std::atomic<SpeakerLayout> m_speaker_layout = Surround6;
  std::atomic<MediaBitness> m_bitness = bit16;
  std::atomic<ALsizei> m_frequency = 48000;

  // Get from settings
  uint32_t m_latency = 64;
  bool m_muted = false;

  // Clocking variables and functions
  DWORD MetGetTime(void);
  REFERENCE_TIME m_rtPrivateTime;
  DWORD m_dwPrevSystemTime;

  DWORD m_msPerTick;
  DWORD m_LastTickTime;
  DWORD m_LastTickTGT;

  DWORD m_SamplesSinceTick;
  DWORD m_SamplesSinceSpike;
  BOOL m_fSpikeAtStart;
  DWORD m_dwLastMet;
  DWORD m_dwLastTGT;

  CCritSec m_csClock;

  IReferenceClock* m_pCurrentRefClock;
  IReferenceClock* m_pPrevRefClock;
};
