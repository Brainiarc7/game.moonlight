/*
 *      Copyright (C) 2015 Anthony Miyaguchi
 *      Copyright (C) 2015 Team XBMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "kodi/libXBMC_addon.h"
#include "kodi/libKODI_game.h"
#include "kodi/xbmc_addon_dll.h"
#include "kodi/kodi_game_dll.h"

#include <vector>

using namespace ADDON;

#if !defined(SAFE_DELETE)
  #define SAFE_DELETE(x)  do { delete (x); (x) = NULL; } while (0)
#endif

CHelper_libXBMC_addon*  XBMC      = NULL;
CHelper_libKODI_game*   FRONTEND  = NULL;

extern "C"
{

ADDON_STATUS ADDON_Create(void* callbacks, void* props)
{
  try
  {
    if (!callbacks || !props)
      throw ADDON_STATUS_UNKNOWN;

    XBMC = new ADDON::CHelper_libXBMC_addon;
    if (!XBMC || !XBMC->RegisterMe(callbacks))
      throw ADDON_STATUS_PERMANENT_FAILURE;

    FRONTEND = new CHelper_libKODI_game;
    if (!FRONTEND || !FRONTEND->RegisterMe(callbacks))
      throw ADDON_STATUS_PERMANENT_FAILURE;
  }
  catch (const ADDON_STATUS& status)
  {
    SAFE_DELETE(XBMC);
    SAFE_DELETE(FRONTEND);
    return status;
  }

  return ADDON_GetStatus();
}

void ADDON_Stop()
{
}

void ADDON_Destroy()
{
  SAFE_DELETE(XBMC);
  SAFE_DELETE(FRONTEND);
}

ADDON_STATUS ADDON_GetStatus()
{
  return XBMC ? ADDON_STATUS_OK : ADDON_STATUS_UNKNOWN;
}

bool ADDON_HasSettings()
{
  return false;
}

unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  if (!settingName || !settingValue)
    return ADDON_STATUS_UNKNOWN;

  return ADDON_STATUS_OK;
}

void ADDON_FreeSettings()
{
}

void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

const char* GetGameAPIVersion(void)
{
  return GAME_API_VERSION;
}

const char* GetMininumGameAPIVersion(void)
{
  return GAME_MIN_API_VERSION;
}

GAME_ERROR LoadGame(const char* url)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR LoadGameSpecial(GAME_TYPE type, const char** urls, size_t num_urls)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR UnloadGame(void)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR Run(void)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR Reset(void)
{
  return GAME_ERROR_FAILED;
}

void ControllerConnected(unsigned int port, bool connected, const game_controller* connected_controller)
{
}

bool InputEvent(unsigned int port, const game_input_event* event)
{
  return false;
}

GAME_ERROR GetSystemAVInfo(game_system_av_info *info)
{
  return GAME_ERROR_FAILED;
}

size_t SerializeSize(void)
{
  return 0;
}

GAME_ERROR Serialize(void *data, size_t size)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR Deserialize(const void *data, size_t size)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CheatReset(void)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CheatSet(unsigned index, bool enabled, const char *code)
{
  return GAME_ERROR_FAILED;
}

GAME_REGION GetRegion(void)
{
  return GAME_REGION_NTSC;
}

void* GetMemoryData(GAME_MEMORY id)
{
  return NULL;
}

size_t GetMemorySize(GAME_MEMORY id)
{
  return 0;
}

GAME_ERROR DiskSetEjectState(GAME_EJECT_STATE ejected)
{
  return GAME_ERROR_FAILED;
}

GAME_EJECT_STATE DiskGetEjectState()
{
  return GAME_NOT_EJECTED;
}

unsigned DiskGetImageIndex()
{
  return 0;
}

GAME_ERROR DiskSetImageIndex(unsigned index)
{
  return GAME_ERROR_FAILED;
}

unsigned DiskGetNumImages()
{
  return 0;
}

GAME_ERROR DiskReplaceImageIndex(unsigned index, const char* url)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR DiskAddImageIndex()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR HwContextReset()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR HwContextDestroy()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR AudioAvailable()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR AudioSetState(bool enabled)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR FrameTimeNotify(game_usec_t usec)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CameraInitialized()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CameraDeinitialized()
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CameraFrameRawBuffer(const uint32_t *buffer, unsigned width, unsigned height, size_t pitch)
{
  return GAME_ERROR_FAILED;
}

GAME_ERROR CameraFrameOpenglTexture(unsigned texture_id, unsigned texture_target, const float *affine)
{
  return GAME_ERROR_FAILED;
}

} // extern "C"