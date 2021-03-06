#include "pch.h"

using namespace std;

using namespace Log;
using namespace StrHelpers;
using namespace FileHelpers;

static IConfig conf;

IPlugin* pHandle;

static vector<wstring> blockedModules;

static LoadLibraryA_T LoadLibraryA_O = LoadLibraryA;
static LoadLibraryW_T LoadLibraryW_O = LoadLibraryW;
static LoadLibraryExA_T LoadLibraryExA_O = LoadLibraryExA;
static LoadLibraryExW_T LoadLibraryExW_O = LoadLibraryExW;

static bool FilterLoadLibraryW(const wstring& lfn)
{
    auto it = find(blockedModules.begin(), blockedModules.end(), lfn);
    if (it != blockedModules.end())
    {
        MESSAGE("Blocking load: %s", StrToNative(lfn).c_str());
        return false;
    }

    return true;
}

static void
InstallHooksIfLoaded()
{
    if (pHandle->GetLoaderVersion() == Loader::Version::D3D9) {
        D3D9::InstallHooksIfLoaded();
    }
    else if (pHandle->GetLoaderVersion() == Loader::Version::DXGI) {
        D3D11::InstallHooksIfLoaded();
    }
    Inet::InstallHooksIfLoaded();
    Window::InstallHooksIfLoaded();
}

static HMODULE
WINAPI
LoadLibraryA_Hook(
    _In_ LPCSTR lpLibFileName
)
{
    try {
        auto fileName(GetPathFileNameA(lpLibFileName));

        if (!FilterLoadLibraryW(ToWString(fileName))) {
            SetLastError(ERROR_MOD_NOT_FOUND);
            return NULL;
        }
    }
    catch (exception& e) {
        MESSAGE("Exception while filtering library: ", StrToNative(e.what()).c_str());
    }

    HMODULE r = LoadLibraryA_O(lpLibFileName);
    InstallHooksIfLoaded();
    return r;
}

static HMODULE
WINAPI
LoadLibraryW_Hook(
    _In_ LPCWSTR lpLibFileName
)
{
    try {
        auto fileName(GetPathFileNameW(lpLibFileName));

        if (!FilterLoadLibraryW(fileName)) {
            SetLastError(ERROR_MOD_NOT_FOUND);
            return NULL;
        }
    }
    catch (exception& e) {
        MESSAGE("Exception while filtering library: ", StrToNative(e.what()).c_str());
    }

    HMODULE r = LoadLibraryW_O(lpLibFileName);
    InstallHooksIfLoaded();
    return r;
}


static HMODULE
WINAPI
LoadLibraryExA_Hook(
    _In_ LPCSTR lpLibFileName,
    _Reserved_ HANDLE hFile,
    _In_ DWORD dwFlags
)
{
    try {
        auto fileName(GetPathFileNameA(lpLibFileName));

        if (!FilterLoadLibraryW(ToWString(fileName))) {
            SetLastError(ERROR_MOD_NOT_FOUND);
            return NULL;
        }
    }
    catch (exception& e) {
        MESSAGE("Exception while filtering library: ", StrToNative(e.what()).c_str());
    }

    HMODULE r = LoadLibraryExA_O(lpLibFileName, hFile, dwFlags);
    InstallHooksIfLoaded();
    return r;
}

static HMODULE
WINAPI
LoadLibraryExW_Hook(
    _In_ LPCWSTR lpLibFileName,
    _Reserved_ HANDLE hFile,
    _In_ DWORD dwFlags
)
{
    try {
        auto fileName(GetPathFileNameW(lpLibFileName));

        if (!FilterLoadLibraryW(fileName)) {
            SetLastError(ERROR_MOD_NOT_FOUND);
            return NULL;
        }
    }
    catch (exception& e) {
        MESSAGE("Exception while filtering library: ", StrToNative(e.what()).c_str());
    }

    HMODULE r = LoadLibraryExW_O(lpLibFileName, hFile, dwFlags);
    InstallHooksIfLoaded();
    return r;
}

extern "C"
{
    bool __cdecl Initialize(IPlugin* h)
    {
        h->SetPluginName("FE");
        h->SetPluginVersion(0, 3, 2);

        pHandle = h;

        auto loaderVersion = h->GetLoaderVersion();

        STD_STRING exePath = StrToNative(h->GetExecutablePath());
        STD_STRING logPath = exePath + _T("\\DLLPluginLogs");
        STD_STRING configPath = exePath + _T("\\DLLPlugins\\FE.ini");

        gLog.Open(logPath.c_str(), _T("FE.log"));
        MESSAGE("Initializing plugin");

        if (conf.Load(StrToStr(configPath)) != 0) {
            MESSAGE("Couldn't load config file");
        }

        DXGI::HasSwapEffect = DXGI::ConfigTranslateSwapEffect(
            conf.Get("DXGI", "SwapEffect", "default"),
            DXGI::DXGISwapEffectInt
        );

        DXGI::HasFormat = conf.Exists("DXGI", "Format");
        if (DXGI::HasFormat) {
            if (conf.Get("DXGI", "Format", "") == "auto") {
                DXGI::FormatAuto = true;
            }
            else {
                DXGI::Format = conf.Get("DXGI", "Format", DXGI_FORMAT_R8G8B8A8_UNORM);
            }
        }

        D3D11::CreateShaderResourceViewRetryOnFail = conf.Exists("DXGI", "CreateShaderResourceViewRetryFormat");
        if (D3D11::CreateShaderResourceViewRetryOnFail) {
            D3D11::CreateShaderResourceViewRetryFormat = conf.Get("DXGI", "CreateShaderResourceViewRetryFormat", DXGI_FORMAT_UNKNOWN);
        }

        DXGI::BufferCount = clamp<UINT>(conf.Get<UINT>("DXGI", "BufferCount", 0), 0U, 8U);
        DXGI::DisplayMode = clamp(conf.Get("DXGI", "DisplayMode", -1), -1, 1);
        DXGI::EnableTearing = conf.Get("DXGI", "EnableTearing", false);
        DXGI::ExplicitRebind = conf.Get("DXGI", "ExplicitRebind", false);
        D3D11::MaxFrameLatency = clamp(conf.Get("DXGI", "MaxFrameLatency", -1), -1, 16);

        STD_STRING formatStr;
        if (DXGI::HasFormat) {
            if (DXGI::FormatAuto) {
                formatStr = _T("auto");
            }
            else {
                formatStr = STD_TOSTR(static_cast<int>(DXGI::Format));
            }
        }
        else {
            formatStr = _T("default");
        }

        STD_STRING swapEffectStr;
        if (DXGI::HasSwapEffect) {
            swapEffectStr = DXGI::GetSwapEffectDesc(DXGI::DXGISwapEffectInt);
        }
        else {
            swapEffectStr = _T("default");
        }

        MESSAGE("DXGI: SwapEffect: %s, Format: %s, BufferCount: %u, Tearing: %d",
            swapEffectStr.c_str(), formatStr.c_str(),
            DXGI::BufferCount, DXGI::EnableTearing);

        float framerateLimit = conf.Get("DXGI", "FramerateLimit", 0.0f);
        if (framerateLimit > 0.0f) {
            DXGI::fps_max = static_cast<long long>((1.0f / framerateLimit) * 1000000.0f);
            MESSAGE("Framerate limit: %.6g", framerateLimit);
        }

        //D3D9::EnableEx = conf.Get("D3D9", "EnableEx", true);
        D3D9::EnableFlip = conf.Get("D3D9", "EnableFlip", false);
        D3D9::PresentIntervalImmediate = conf.Get("D3D9", "PresentIntervalImmediate", false);
        D3D9::BufferCount = clamp<int32_t>(conf.Get<int32_t>("D3D9", "BufferCount", -1), -1, D3DPRESENT_BACK_BUFFERS_MAX_EX);
        D3D9::MaxFrameLatency = clamp<int32_t>(conf.Get<int32_t>("D3D9", "MaxFrameLatency", -1), -1, 20);
        D3D9::CreateTextureUsageDynamic = conf.Get("D3D9", "CreateTextureUsageDynamic", false);
        D3D9::CreateTextureClearUsageFlags = conf.Get<UINT>("D3D9", "CreateTextureClearUsageFlags", 0x0U);
        D3D9::CreateIndexBufferUsageDynamic = conf.Get("D3D9", "CreateIndexBufferUsageDynamic", false);
        D3D9::CreateVertexBufferUsageDynamic = conf.Get("D3D9", "CreateVertexBufferUsageDynamic", false);
        D3D9::CreateCubeTextureUsageDynamic = conf.Get("D3D9", "CreateCubeTextureUsageDynamic", false);
        D3D9::CreateVolumeTextureUsageDynamic = conf.Get("D3D9", "CreateVolumeTextureUsageDynamic", false);
        D3D9::ForceAdapter = conf.Get<int32_t>("D3D9", "ForceAdapter", -1);
        D3D9::DisplayMode = clamp<int32_t>(conf.Get<int32_t>("D3D9", "DisplayMode", -1), -1, 1);

        D3D9::HasFormat = conf.Exists("D3D9", "Format");
        if (D3D9::HasFormat) {
            if (conf.Get("D3D9", "Format", "") == "auto") {
                D3D9::FormatAuto = true;
            }
            else {
                D3D9::D3DFormat = conf.Get("D3D9", "Format", D3DFMT_X8R8G8B8);
            }
        }

        uint32_t numBlockedModules = SplitStringW(
            ToWString(conf.Get("DLL", "BlockedModules", "")),
            L',', blockedModules);

        Inet::blockConnections = conf.Get("Net", "BlockConnections", false);
        Inet::blockListen = conf.Get("Net", "BlockListen", false);
        Inet::blockInternetOpen = conf.Get("Net", "BlockInternetOpen", false);
        Inet::blockDNSResolve = conf.Get("Net", "BlockDNSResolve", false);

        Window::BorderlessUpscaling = conf.Get("Window", "BorderlessUpscaling", false);
        Window::ForceBorderless = conf.Get("Window", "ForceBorderless", false) && (D3D9::DisplayMode == 1 || DXGI::DisplayMode == 0);
        Window::CursorFix = conf.Get("Window", "CursorFix", false);

        auto hookIf = h->GetHookInterface();

        //MESSAGE(":: %u", (*hookIf).GetRefCount());

        if (numBlockedModules > 0) {
            MESSAGE("%u module(s) in blocklist", numBlockedModules);
        }

        hookIf->Register(&(PVOID&)LoadLibraryA_O, (PVOID)LoadLibraryA_Hook);
        hookIf->Register(&(PVOID&)LoadLibraryW_O, (PVOID)LoadLibraryW_Hook);
        hookIf->Register(&(PVOID&)LoadLibraryExA_O, (PVOID)LoadLibraryExA_Hook);
        hookIf->Register(&(PVOID&)LoadLibraryExW_O, (PVOID)LoadLibraryExW_Hook);

        if (Inet::blockConnections) {
            uint32_t numAllowedPorts = SplitString(
                StrToNative(conf.Get("Net", "AllowedPorts", "")),
                _T(','), Inet::allowedPorts);

            MESSAGE("Blocking network connections (%u port(s) in allow list)", numAllowedPorts);
        }

        if (Inet::blockListen) {
            MESSAGE("Blocking network listen");
        }

        if (Inet::blockDNSResolve) {
            uint32_t numAllowedHosts = SplitStringW(
                ToWString(conf.Get("Net", "AllowedHosts", "")),
                L',', Inet::allowedHosts);

            MESSAGE("Blocking DNS resolve (%u host(s) in allow list)", numAllowedHosts);
        }

        if (Inet::blockInternetOpen) {
            MESSAGE("Blocking InternetOpen");
        }

        if (!IHookLogged(hookIf, &gLog).InstallHooks()) {
            MESSAGE("Detours installed");
        }

        InstallHooksIfLoaded();

        if (loaderVersion == Loader::Version::DXGI) {
            DXGI::InstallProxyOverrides(h);
        }
        else if (loaderVersion == Loader::Version::D3D9) {
            D3D9::InstallProxyOverrides(h);
        }

        return true;
    }
}

