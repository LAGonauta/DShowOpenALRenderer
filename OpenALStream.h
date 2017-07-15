// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <thread>
#include <concurrent_queue.h>

#include <include/OpenAL/al.h>
#include <include/OpenAL/alc.h>

#include "streams.h"

// OpenAL requires a minimum of two buffers, three or more recommended
#define OAL_BUFFERS 3
#define OAL_MAX_FRAMES 65536
#define STEREO_CHANNELS 2
#define SURROUND_CHANNELS 6  // number of channels in surround mode
#define SIZE_SHORT 2
#define SIZE_INT32 4
#define SIZE_FLOAT 4  // size of a float in bytes
#define FRAME_STEREO_SHORT STEREO_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_FLOAT SURROUND_CHANNELS* SIZE_FLOAT
#define FRAME_SURROUND_SHORT SURROUND_CHANNELS* SIZE_SHORT
#define FRAME_SURROUND_INT32 SURROUND_CHANNELS* SIZE_INT32

// From AL_EXT_float32
#ifndef AL_FORMAT_STEREO_FLOAT32
#define AL_FORMAT_STEREO_FLOAT32 0x10011
#endif

// From AL_EXT_MCFORMATS
#ifndef AL_FORMAT_51CHN16
#define AL_FORMAT_51CHN16 0x120B
#endif
#ifndef AL_FORMAT_51CHN32
#define AL_FORMAT_51CHN32 0x120C
#endif

// Only X-Fi on Windows supports the alext AL_FORMAT_STEREO32 alext for now,
// but it is not documented or in "OpenAL/include/al.h".
#ifndef AL_FORMAT_STEREO32
#define AL_FORMAT_STEREO32 0x1203
#endif

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
  REFERENCE_TIME GetPrivateTime() override;
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

  HRESULT Pause();
  HRESULT Stop();
  HRESULT setSpeakerLayout(SpeakerLayout layout);
  SpeakerLayout getSpeakerLayout();
  HRESULT setFrequency(uint32_t frequency);
  uint32_t getFrequency();
  HRESULT setBitness(MediaBitness bitness);
  MediaBitness getBitness();

private:
  std::thread m_thread;
  bool m_pin_locked = false;
  bool m_run_thread = false;

  void SoundLoop();
  void SetVolume(int volume);
  void Destroy();

  uint32_t num_buffers = 3;

  std::vector<ALuint> m_buffers;
  ALuint m_source = 0;
  ALfloat m_volume = 1.0f;

  CMixer* m_mixer;
  SpeakerLayout m_speaker_layout = Surround6;
  MediaBitness m_bitness = bit16;
  uint32_t m_frequency = 48000;

  // Get from settings
  uint32_t m_latency = 30;
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
