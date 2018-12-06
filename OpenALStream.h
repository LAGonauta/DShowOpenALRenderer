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
  COpenALStream(LPUNKNOWN pUnk, HRESULT* phr, COpenALFilter* base_filter);

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

  HRESULT StartStreaming();
  HRESULT StopStreaming();

  HRESULT Receive(IMediaSample* pIn);
  HRESULT Stop();

  HRESULT checkMediaType(const WAVEFORMATEX* wave_format);
  HRESULT setMediaType(const CMediaType* pmt);
  SpeakerLayout getSpeakerLayout();
  MediaBitness getBitness();

  HRESULT ResetBuffer();

  // In milliseconds
  REFERENCE_TIME getSampleTime();

private:
  STDMETHODIMP isValid();

  void SetVolume(int volume);
  void Destroy();
  void PushToDevice(IMediaSample *pMediaSample);
  ALenum CheckALError(std::string desc);
  std::vector<MediaBitness> getSupportedBitness();
  std::vector<SpeakerLayout> getSupportedSpeakerLayout();
  SpeakerLayout ChannelsToSpeakerLayout(size_t num_channels);
  MediaBitness WaveformatToBitness(const WAVEFORMATEX* wave_format);

  uint32_t num_buffers = OAL_BUFFERS;
  uint32_t num_buffers_queued = 0;

  std::vector<ALuint> m_buffers;
  std::deque<size_t> m_buffer_size;
  size_t m_total_played = 0;
  ALuint m_source = 0;
  ALfloat m_volume = 1.0f;

  SpeakerLayout m_speaker_layout = Surround6;
  MediaBitness m_bitness = bit16;
  ALsizei m_frequency = 48000;

  // Get from settings
  uint32_t m_latency = 128;
  bool m_muted = false;

  // For CBaseClock
  COpenALFilter* m_base_filter;
  REFERENCE_TIME m_start_time;

  unsigned int next_buffer = 0;
  uint32_t past_frequency = m_frequency;
  SpeakerLayout past_speaker_layout = m_speaker_layout;
  MediaBitness past_bitness = m_bitness;

  std::atomic<bool> m_bStreaming; // Are we currently streaming

  int m_LastMediaSampleSize;      // Size of last MediaSample
  size_t m_desired_bytes = 0;

  int m_nChannels;                // number of active channels
  int m_nSamplesPerSec;           // Samples per second
  int m_nBitsPerSample;           // Number bits per sample
  bool m_is_float;
  int m_nBlockAlign;              // Alignment on the samples
};
