#pragma once

#ifndef __STREAMS__
#include <streams.h>
#endif
#include <atomic>
#include <comdef.h>
#include <concurrent_queue.h>
#include <initguid.h>
#include <mmreg.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <windows.h>
#include <queue>

#include "OpenALAudioRenderer.h"
#include "OpenALAudioManager.h"
#include "CAudioInputPin.h"
#include "COpenALFilter.h"
#include "OpenALStream.h"
