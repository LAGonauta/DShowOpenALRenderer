// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <include/OpenAL/al.h>
#include <include/OpenAL/alc.h>

// OpenAL requires a minimum of two buffers, three or more recommended
constexpr size_t OAL_BUFFERS = 8;

class COpenALFilter;

class COpenALStream : public CBasicAudio, public CBaseReferenceClock
{
  friend class COpenALFilter;
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
  COpenALStream(CMixer* audioMixer, LPUNKNOWN pUnk, HRESULT* phr, COpenALFilter* base_filter);

  // We must make this time depend on the sound card buffers latter,
  // not on the system clock
  REFERENCE_TIME GetPrivateTime() override;
  //void SetSyncSource(IReferenceClock *pClock);

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
  std::queue<size_t> m_buffer_size;
  std::atomic<size_t> m_total_played = 0;
  ALuint m_source = 0;
  std::atomic<ALfloat> m_volume = 1.0f;

  CMixer* m_mixer;
  std::atomic<SpeakerLayout> m_speaker_layout = Surround6;
  std::atomic<MediaBitness> m_bitness = bit16;
  std::atomic<ALsizei> m_frequency = 48000;

  // Get from settings
  uint32_t m_latency = 128;
  bool m_muted = false;

  // For CBaseClock
  COpenALFilter* m_base_filter;
  REFERENCE_TIME m_start_time;
};
