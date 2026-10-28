/* RA_Interface.h implementation for builds that link RAIntegration statically (macOS/Linux).
 *
 * The Windows build uses RAInterface/RA_Interface.cpp, which downloads and loads RA_Integration.dll and
 * forwards each RA_* call to the matching _RA_* export. Here the integration is compiled in, so the calls
 * go straight to the exports and there is no DLL to download, version-check or unload. */

#include "RA_Interface.h"

#include "cmake/msvc_compat.h" // SAL annotations used by Exports.hh
#include "Exports.hh"

#include <cstring>
#include <stdexcept>

static bool g_bInitialized = false;

void RA_Init(HWND hMainHWND, int nEmulatorID, const char* sClientVersion)
{
    g_bInitialized = (_RA_InitI(hMainHWND, nEmulatorID, sClientVersion) != 0);
}

void RA_InitClient(HWND hMainHWND, const char* sClientName, const char* sClientVersion)
{
    g_bInitialized = (_RA_InitClient(hMainHWND, sClientName, sClientVersion) != 0);
}

void RA_InstallSharedFunctions(int (*)(void), void (*fpCauseUnpause)(void), void (*fpCausePause)(void),
                               void (*fpRebuildMenu)(void), void (*fpEstimateTitle)(char*),
                               void (*fpResetEmulation)(void), void (*fpLoadROM)(const char*))
{
    if (g_bInitialized)
            _RA_InstallSharedFunctionsExt(nullptr, fpCauseUnpause, fpCausePause, fpRebuildMenu, fpEstimateTitle,
                                  fpResetEmulation, fpLoadROM);
}

void RA_SetForceRepaint(int bEnable) { if (g_bInitialized) _RA_SetForceRepaint(bEnable); }
HMENU RA_CreatePopupMenu(void) { return nullptr; } // no Win32 menus; use RA_GetPopupMenuItems
int RA_GetPopupMenuItems(RA_MenuItem* pItems) { return g_bInitialized ? _RA_GetPopupMenuItems(pItems) : 0; }
void RA_InvokeDialog(LPARAM nID) { if (g_bInitialized) _RA_InvokeDialog(nID); }
void RA_SetUserAgentDetail(const char* sDetail) { if (g_bInitialized) _RA_SetUserAgentDetail(sDetail); }
void RA_AttemptLogin(int bBlocking) { if (g_bInitialized) _RA_AttemptLogin(bBlocking); }
void RA_SetConsoleID(unsigned int nConsoleID) { if (g_bInitialized) _RA_SetConsoleID(nConsoleID); }
void RA_ClearMemoryBanks(void) { if (g_bInitialized) _RA_ClearMemoryBanks(); }

void RA_InstallMemoryBank(int nBankID, RA_ReadMemoryFunc pReader, RA_WriteMemoryFunc pWriter, int nBankSize)
{
    if (g_bInitialized)
            _RA_InstallMemoryBank(nBankID, reinterpret_cast<void*>(pReader), reinterpret_cast<void*>(pWriter), nBankSize);
}

void RA_InstallMemoryBankBlockReader(int nBankID, RA_ReadMemoryBlockFunc pReader)
{
    if (g_bInitialized)
            _RA_InstallMemoryBankBlockReader(nBankID, reinterpret_cast<void*>(pReader));
}

void RA_Shutdown(void)
{
    if (!g_bInitialized)
        return;

    try
    {
        _RA_Shutdown();
    }
    catch (std::runtime_error&)
    {
    }

    g_bInitialized = false;
}

int RA_IsOverlayFullyVisible(void) { return g_bInitialized ? _RA_IsOverlayFullyVisible() : 0; }
void RA_SetPaused(bool bIsPaused) { if (g_bInitialized) _RA_SetPaused(bIsPaused ? 1 : 0); }
void RA_NavigateOverlay(struct ControllerInput* pInput) { if (g_bInitialized) _RA_NavigateOverlay(pInput); }

void RA_UpdateRenderOverlay(HDC, struct ControllerInput* pInput, float, RECT*, bool, bool)
{
    if (g_bInitialized)
        _RA_NavigateOverlay(pInput);
}

void RA_UpdateHWnd(HWND hMainHWND) { if (g_bInitialized) _RA_UpdateHWnd(hMainHWND); }
unsigned int RA_IdentifyRom(BYTE* pROMData, unsigned int nROMSize) { return g_bInitialized ? _RA_IdentifyRom(pROMData, nROMSize) : 0; }
unsigned int RA_IdentifyHash(const char* sHash) { return g_bInitialized ? _RA_IdentifyHash(sHash) : 0; }
void RA_ActivateGame(unsigned int nGameId) { if (g_bInitialized) _RA_ActivateGame(nGameId); }
void RA_OnLoadNewRom(BYTE* pROMData, unsigned int nROMSize) { if (g_bInitialized) _RA_OnLoadNewRom(pROMData, nROMSize); }
int RA_ConfirmLoadNewRom(int bIsQuitting) { return g_bInitialized ? _RA_ConfirmLoadNewRom(bIsQuitting) : 1; }
void RA_DoAchievementsFrame(void) { if (g_bInitialized) _RA_DoAchievementsFrame(); }
void RA_SuspendRepaint(void) { if (g_bInitialized) _RA_SuspendRepaint(); }
void RA_ResumeRepaint(void) { if (g_bInitialized) _RA_ResumeRepaint(); }
void RA_HandleHTTPResults(void) {}
void RA_UpdateAppTitle(const char* sCustomMessage) { if (g_bInitialized) _RA_UpdateAppTitle(sCustomMessage); }
const char* RA_UserName(void) { return g_bInitialized ? _RA_UserName() : ""; }
int RA_HardcoreModeIsActive(void) { return g_bInitialized ? _RA_HardcoreModeIsActive() : 0; }

int RA_WarnDisableHardcore(const char* sActivity)
{
    // If Hardcore mode not active, allow the activity.
    if (!RA_HardcoreModeIsActive())
        return 1;

    // the integration displays a yes/no prompt and disables hardcore if the user agrees
    return _RA_WarnDisableHardcore(sActivity);
}

void RA_DisableHardcore(void)
{
    // passing nullptr just disables hardcore mode without prompting.
    if (g_bInitialized)
        _RA_WarnDisableHardcore(nullptr);
}

void RA_OnReset(void) { if (g_bInitialized) _RA_OnReset(); }
void RA_OnSaveState(const char* sFilename) { if (g_bInitialized) _RA_OnSaveState(sFilename); }
void RA_OnLoadState(const char* sFilename) { if (g_bInitialized) _RA_OnLoadState(sFilename); }
int RA_CaptureState(char* pBuffer, int nBufferSize) { return g_bInitialized ? _RA_CaptureState(pBuffer, nBufferSize) : 0; }
void RA_RestoreState(const char* pBuffer) { if (g_bInitialized) _RA_RestoreState(pBuffer); }

// ---- host integration (RA_Interface_Host.h) ----

#include "RA_Interface_Host.h"

void RA_SetBaseDirectory(const char* utf8Path) { _RA_SetBaseDirectory(utf8Path); }
void RA_ProcessUIThreadActions(void) { _RA_ProcessUIThreadActions(); }

void RA_SetClipboardHandlers(void (*setText)(const char*), const char* (*getText)(void))
{
    if (g_bInitialized)
        _RA_SetClipboardHandlers(setText, getText);
}

void RA_SetAudioHandlers(void (*playFile)(const char*), void (*beep)(void))
{
    if (g_bInitialized)
        _RA_SetAudioHandlers(playFile, beep);
}

void RA_SetOverlayImageHandler(void (*handleImage)(const uint32_t*, int, int))
{
    _RA_SetOverlayImageHandler(handleImage);
}

void RA_SetFileDialogHandler(int (*showFileDialog)(int, const char*, const char*, const char*, const char*, char*, int))
{
    _RA_SetFileDialogHandler(showFileDialog);
}
