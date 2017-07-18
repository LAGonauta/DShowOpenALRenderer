// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <include/OpenAL/al.h>
#include <include/OpenAL/alc.h>

#ifndef __STREAMS__
#include "streams.h"
#endif

// From AL_EXT_float32
#ifndef AL_FORMAT_STEREO_FLOAT32
#define AL_FORMAT_MONO_FLOAT32 0x10010
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

// OpenAL requires a minimum of two buffers, three or more recommended
const size_t OAL_BUFFERS = 2;

const size_t MONO_CHANNELS = 1;
const size_t STEREO_CHANNELS = 2;
const size_t QUAD_CHANNELS = 4;
const size_t SURROUND6_CHANNELS = 6;
const size_t SURROUND8_CHANNELS = 8;

const size_t SIZE_BYTE = 1;
const size_t SIZE_SHORT = 2;
const size_t SIZE_INT32 = 4;
const size_t SIZE_FLOAT = 4;  // size in bytes

const size_t FRAME_MONO_BYTE = MONO_CHANNELS * SIZE_BYTE;
const size_t FRAME_MONO_SHORT = MONO_CHANNELS * SIZE_SHORT;
const size_t FRAME_MONO_INT32 = MONO_CHANNELS * SIZE_INT32;
const size_t FRAME_MONO_FLOAT = MONO_CHANNELS * SIZE_FLOAT;

const size_t FRAME_STEREO_BYTE = STEREO_CHANNELS * SIZE_BYTE;
const size_t FRAME_STEREO_SHORT = STEREO_CHANNELS * SIZE_SHORT;
const size_t FRAME_STEREO_INT32 = STEREO_CHANNELS * SIZE_INT32;
const size_t FRAME_STEREO_FLOAT = STEREO_CHANNELS * SIZE_FLOAT;

const size_t FRAME_QUAD_BYTE = QUAD_CHANNELS * SIZE_BYTE;
const size_t FRAME_QUAD_SHORT = QUAD_CHANNELS * SIZE_SHORT;
const size_t FRAME_QUAD_INT32 = QUAD_CHANNELS * SIZE_INT32;
const size_t FRAME_QUAD_FLOAT = QUAD_CHANNELS * SIZE_FLOAT;

const size_t FRAME_SURROUND6_BYTE = SURROUND6_CHANNELS * SIZE_BYTE;
const size_t FRAME_SURROUND6_SHORT = SURROUND6_CHANNELS * SIZE_SHORT;
const size_t FRAME_SURROUND6_INT32 = SURROUND6_CHANNELS * SIZE_INT32;
const size_t FRAME_SURROUND6_FLOAT = SURROUND6_CHANNELS * SIZE_FLOAT;

const size_t FRAME_SURROUND8_BYTE = SURROUND8_CHANNELS * SIZE_BYTE;
const size_t FRAME_SURROUND8_SHORT = SURROUND8_CHANNELS * SIZE_SHORT;
const size_t FRAME_SURROUND8_INT32 = SURROUND8_CHANNELS * SIZE_INT32;
const size_t FRAME_SURROUND8_FLOAT = SURROUND8_CHANNELS * SIZE_FLOAT;

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
  std::vector<MediaBitness> getSupportedBitness();
  std::vector<SpeakerLayout> getSupportedSpeakerLayout();

private:
  std::thread m_thread;
  bool m_run_thread = false;

  void SoundLoop();
  void SetVolume(int volume);
  void Destroy();
  ALenum CheckALError(std::string desc);

  uint32_t num_buffers = OAL_BUFFERS;

  std::vector<ALuint> m_buffers;
  ALuint m_source = 0;
  ALfloat m_volume = 1.0f;

  CMixer* m_mixer;
  SpeakerLayout m_speaker_layout = Surround6;
  MediaBitness m_bitness = bit16;
  uint32_t m_frequency = 48000;

  // Get from settings
  uint32_t m_latency = 48;
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
