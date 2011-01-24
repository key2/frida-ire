#ifndef __CLOUD_SPY_PLUGIN_H__
#define __CLOUD_SPY_PLUGIN_H__

#define VC_EXTRALEAN
#include <windows.h>
#include <tchar.h>

#define CLOUD_SPY_ATTACHPOINT() \
  MessageBox (NULL, _T (__FUNCTION__), _T (__FUNCTION__), MB_ICONINFORMATION | MB_OK)

#endif