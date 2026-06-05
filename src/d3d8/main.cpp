#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>

void Log(const char* msg) {
    FILE* f = fopen("proxy8_log.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

typedef void* (WINAPI* Direct3DCreate8_t)(UINT);
Direct3DCreate8_t Real_Direct3DCreate8 = nullptr;

typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(void*, UINT, DWORD, HWND, DWORD, DWORD*, void**);
typedef HRESULT(STDMETHODCALLTYPE* SetTextureStageState_t)(void*, DWORD, DWORD, DWORD);

CreateDevice_t Original_CreateDevice = nullptr;
SetTextureStageState_t Trampoline_SetTextureStageState = nullptr;

bool systemLoaded = false;
bool inlineHooksInstalled = false;

// trampoline creation function (fixes lags)
void* CreateTrampoline(void* target, void* detour) {
    void* trampoline = VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) return nullptr;

    DWORD oldProtect;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

    // copying original bytes
    memcpy(trampoline, target, 5);

    // jump back into directx (with correct math)
    BYTE jmpBack[5] = { 0xE9, 0, 0, 0, 0 };
    DWORD relJmpBack = ((DWORD)target + 5) - ((DWORD)trampoline + 10);
    memcpy(&jmpBack[1], &relJmpBack, 4);
    memcpy((BYTE*)trampoline + 5, jmpBack, 5);

    // jump from directX in our spy
    BYTE jmpDetour[5] = { 0xE9, 0, 0, 0, 0 };
    DWORD relJmpDetour = (DWORD)detour - (DWORD)target - 5;
    memcpy(&jmpDetour[1], &relJmpDetour, 4);
    memcpy(target, jmpDetour, 5);

    VirtualProtect(target, 5, oldProtect, &oldProtect);

    return trampoline;
}

// smart filter hook
HRESULT STDMETHODCALLTYPE hkSetTextureStageState(void* pDev, DWORD Stage, DWORD Type, DWORD Value) {
    // 16 - mag (upscaling), 17 - min (downscaling)
    if (Type == 16 || Type == 17) {
        Value = 1; // force d3dtexf_point
    }
    // 18 - mip (mipmaps) - it's what broke the scale in sh3
    else if (Type == 18) {
        // set point (1) only if the game itself hasn't disabled it (0)
        if (Value != 0) {
            Value = 1; 
        }
    }
    
    return Trampoline_SetTextureStageState(pDev, Stage, Type, Value);
}

// device hook
HRESULT STDMETHODCALLTYPE hkCreateDevice(void* pD3D8, UINT Adapter, DWORD DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, DWORD* pParams, void** ppDevice) {

    // don't touch pparams
    // the silent hill 3 engine is too sensitive to changing aa settings

    HRESULT hr = Original_CreateDevice(pD3D8, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParams, ppDevice);

    if (SUCCEEDED(hr) && ppDevice && *ppDevice && !inlineHooksInstalled) {
        void* pDevice = *ppDevice;
        DWORD* pDeviceVTable = (DWORD*)*((DWORD*)pDevice);

        // in dx8 the filter function is at index 63
        void* rawSetTextureStageState = (void*)pDeviceVTable[63];

        Log("[PROXY8] Silent Hill 3 detected. Installing smart trampoline...");
        Trampoline_SetTextureStageState = (SetTextureStageState_t)CreateTrampoline(rawSetTextureStageState, (void*)&hkSetTextureStageState);

        inlineHooksInstalled = true;
        Log("[PROXY8] Trampoline installed successfully! The zoom bug should be gone.");
    }
    return hr;
}

// initialization (no-sdk)
void InitSystemD3D8() {
    if (systemLoaded) return;
    char syspath[MAX_PATH];
    GetSystemDirectoryA(syspath, MAX_PATH);
    lstrcatA(syspath, "\\d3d8.dll");
    HMODULE hRealD3D8 = LoadLibraryA(syspath);
    if (hRealD3D8) {
        Real_Direct3DCreate8 = (Direct3DCreate8_t)GetProcAddress(hRealD3D8, "Direct3DCreate8");
        systemLoaded = true;
    }
}

extern "C" void* WINAPI Direct3DCreate8(UINT SDKVersion) {
    InitSystemD3D8();
    if (!Real_Direct3DCreate8) return nullptr;
    void* pD3D8 = Real_Direct3DCreate8(SDKVersion);

    if (pD3D8) {
        DWORD* pD3D8VTable = (DWORD*)*((DWORD*)pD3D8);
        DWORD oldProtect;

        // Индекс 15 - CreateDevice
        if (pD3D8VTable[15] != (DWORD)&hkCreateDevice) {
            if (VirtualProtect(&pD3D8VTable[15], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                Original_CreateDevice = (CreateDevice_t)pD3D8VTable[15];
                pD3D8VTable[15] = (DWORD)&hkCreateDevice;
                VirtualProtect(&pD3D8VTable[15], sizeof(DWORD), oldProtect, &oldProtect);
            }
        }
    }
    return pD3D8;
}

// ea games stubs (won't interfere — keep them for compatibility with other games)
extern "C" HRESULT WINAPI ValidatePixelShader(DWORD* pixelshader, DWORD* reserved1, BOOL flag, DWORD* toto) { return 0; }
extern "C" HRESULT WINAPI ValidateVertexShader(DWORD* vertexshader, DWORD* reserved1, DWORD* reserved2, BOOL flag, DWORD* toto) { return 0; }
extern "C" void WINAPI DebugSetMute() {}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls((HMODULE)hinstDLL);
        DeleteFileA("proxy8_log.txt");
        Log("======================================");
        Log("[PROXY8] 'Jewel' version (Smart Trampoline) loaded.");
    }
    return TRUE;
}
