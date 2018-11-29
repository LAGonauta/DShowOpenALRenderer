#include "BaseHeader.h"

// Setup data

const AMOVIESETUP_MEDIATYPE sudPinTypes =
{
  &MEDIATYPE_Audio,           // Major type
  &MEDIASUBTYPE_NULL          // Minor type
};

const AMOVIESETUP_PIN sudPins =
{
  L"Input",                   // Pin string name
  TRUE,                       // Is it rendered
  FALSE,                      // Is it an output
  FALSE,                      // Allowed zero pins
  FALSE,                      // Allowed many
  &CLSID_NULL,                // Connects to filter
  nullptr,                    // Connects to pin
  1,                          // Number of pins types
  &sudPinTypes };             // Pin information

const AMOVIESETUP_FILTER sudOALRend =
{
  &CLSID_OALRend,             // Filter CLSID
  L"OpenAL Renderer",         // String name
  MERIT_DO_NOT_USE,           // Filter merit
  1,                          // Number pins
  &sudPins                    // Pin details
};

// List of class IDs and creator functions for class factory

CFactoryTemplate g_Templates[] = {
  { L"OpenAL Renderer"
  , &CLSID_OALRend
  , (LPFNNewCOMObject)COpenALFilter::CreateInstance
  , nullptr
  , &sudOALRend }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterServer
//
// Handles DLL registry
//
STDAPI DllRegisterServer()
{
  return AMovieDllRegisterServer2(TRUE);
} // DllRegisterServer

  //
  // DllUnregisterServer
  //
STDAPI DllUnregisterServer()
{
  return AMovieDllRegisterServer2(FALSE);
} // DllUnregisterServer

  //
  // DllEntryPoint
  //
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule,
  DWORD  dwReason,
  LPVOID lpReserved)
{
  return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}

void DebugPrintf(const wchar_t *str, ...)
{
  wchar_t buf[2048];

  va_list ptr;
  va_start(ptr, str);
  vswprintf_s(buf, 2048, str, ptr);

  OutputDebugString(buf);
}