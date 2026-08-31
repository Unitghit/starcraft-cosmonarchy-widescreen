#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

constexpr DWORD MPQDRAFT_MAX_PATH = 264;

#pragma pack(push, 1)
struct MPQDRAFTPLUGINMODULE {
    DWORD dwComponentID;
    DWORD dwModuleID;
    BOOL bExecute;
    char szModuleFileName[MPQDRAFT_MAX_PATH];
};
#pragma pack(pop)

struct IMPQDraftServer {
    virtual BOOL WINAPI GetPluginModule(
        DWORD dwPluginID,
        DWORD dwModuleID,
        LPSTR lpszFileName) = 0;
};

struct IMPQDraftPlugin {
    virtual BOOL WINAPI Identify(LPDWORD lpdwPluginID) = 0;
    virtual BOOL WINAPI GetPluginName(LPSTR lpszPluginName, DWORD nNameBufferLength) = 0;
    virtual BOOL WINAPI CanPatchExecutable(LPCSTR lpszEXEFileName) = 0;
    virtual BOOL WINAPI Configure(HWND hParentWnd) = 0;
    virtual BOOL WINAPI ReadyForPatch() = 0;
    virtual BOOL WINAPI GetModules(MPQDRAFTPLUGINMODULE* lpPluginModules, LPDWORD lpnNumModules) = 0;
    virtual BOOL WINAPI InitializePlugin(IMPQDraftServer* lpMPQDraftServer) = 0;
    virtual BOOL WINAPI TerminatePlugin() = 0;
};

