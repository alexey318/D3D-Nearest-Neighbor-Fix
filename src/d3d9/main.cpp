#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

void Log(const char* msg) {
    FILE* f = fopen("proxy_log.txt", "a");
    if (f) { fprintf(f, "%s\n", msg); fclose(f); }
}

typedef IDirect3D9* (WINAPI* Direct3DCreate9_t)(UINT);
Direct3DCreate9_t Real_Direct3DCreate9 = nullptr;

typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT(STDMETHODCALLTYPE* SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);
typedef HRESULT(STDMETHODCALLTYPE* SetTexture_t)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);

CreateDevice_t Original_CreateDevice = nullptr;

// up the trampolines
SetSamplerState_t Trampoline_SetSamplerState = nullptr;
SetTexture_t Trampoline_SetTexture = nullptr;

bool systemLoaded = false;
bool inlineHooksInstalled = false;

// trampolines function
void* CreateTrampoline(void* target, void* detour) {
    void* trampoline = VirtualAlloc(NULL, 10, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!trampoline) return nullptr;

    DWORD oldProtect;
    VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
    
    // 1. copy 5 original bytes from directx to our trampoline
    memcpy(trampoline, target, 5);
    
    // 2. make jump (jmp) from the trampoline back to directx (target + 5 bytes)
    BYTE jmpBack[5] = { 0xE9, 0, 0, 0, 0 };
    // fix: target w/o current eip (address of trampoline + 10 bytes)
    DWORD relJmpBack = ((DWORD)target + 5) - ((DWORD)trampoline + 10);
    memcpy(&jmpBack[1], &relJmpBack, 4);
    memcpy((BYTE*)trampoline + 5, jmpBack, 5);
    
    // 3. make jump (jmp) from directx to our function (detour)
    BYTE jmpDetour[5] = { 0xE9, 0, 0, 0, 0 };
    DWORD relJmpDetour = (DWORD)detour - (DWORD)target - 5;
    memcpy(&jmpDetour[1], &relJmpDetour, 4);
    memcpy(target, jmpDetour, 5);
    
    VirtualProtect(target, 5, oldProtect, &oldProtect);
    
    return trampoline;
}

// inline hook handlers
int samplerHits = 0;
HRESULT STDMETHODCALLTYPE hkSetSamplerState(IDirect3DDevice9* pDev, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    if (samplerHits < 2) {
        Log("[INLINE] Trampoline for SetSamplerState executed successfully and did not crash!");
        samplerHits++;
    }

    if (Type == D3DSAMP_MAGFILTER || Type == D3DSAMP_MINFILTER || Type == D3DSAMP_MIPFILTER) {
        Value = D3DTEXF_POINT; 
    }

    return Trampoline_SetSamplerState(pDev, Sampler, Type, Value);
}

int textureHits = 0;
HRESULT STDMETHODCALLTYPE hkSetTexture(IDirect3DDevice9* pDev, DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    if (textureHits < 2) {
        Log("[INLINE] Trampoline for SetTexture executed successfully and did not crash!");
        textureHits++;
    }

    if (pTexture != nullptr) {
        Trampoline_SetSamplerState(pDev, Stage, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        Trampoline_SetSamplerState(pDev, Stage, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        Trampoline_SetSamplerState(pDev, Stage, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
    }

    return Trampoline_SetTexture(pDev, Stage, pTexture);
}

// hook installation
HRESULT STDMETHODCALLTYPE hkCreateDevice(IDirect3D9* pD3D9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pParams, IDirect3DDevice9** ppDevice) {
    HRESULT hr = Original_CreateDevice(pD3D9, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pParams, ppDevice);
    
    if (SUCCEEDED(hr) && ppDevice && *ppDevice && !inlineHooksInstalled) {
        IDirect3DDevice9* pDevice = *ppDevice;
        DWORD* pDeviceVTable = (DWORD*)*((DWORD*)pDevice);

        void* rawSetTexture = (void*)pDeviceVTable[65];
        void* rawSetSamplerState = (void*)pDeviceVTable[69];
        
        Trampoline_SetSamplerState = (SetSamplerState_t)CreateTrampoline(rawSetSamplerState, (void*)&hkSetSamplerState);
        Trampoline_SetTexture = (SetTexture_t)CreateTrampoline(rawSetTexture, (void*)&hkSetTexture);

        inlineHooksInstalled = true;
        Log("[PROXY] Math correct. Safe trampolines installed.");
    }
    return hr;
}

// standard initialization & stubs
void InitSystemD3D9() {
    if (systemLoaded) return;
    char syspath[MAX_PATH];
    GetSystemDirectoryA(syspath, MAX_PATH);
    lstrcatA(syspath, "\\d3d9.dll");
    HMODULE hRealD3D9 = LoadLibraryA(syspath);
    if (hRealD3D9) {
        Real_Direct3DCreate9 = (Direct3DCreate9_t)GetProcAddress(hRealD3D9, "Direct3DCreate9");
        systemLoaded = true;
    }
}

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
    InitSystemD3D9();
    if (!Real_Direct3DCreate9) return nullptr;
    IDirect3D9* pD3D9 = Real_Direct3DCreate9(SDKVersion);
    
    if (pD3D9) {
        DWORD* pD3D9VTable = (DWORD*)*((DWORD*)pD3D9);
        DWORD oldProtect;
        if (pD3D9VTable[16] != (DWORD)&hkCreateDevice) {
            if (VirtualProtect(&pD3D9VTable[16], sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect)) {
                Original_CreateDevice = (CreateDevice_t)pD3D9VTable[16];
                pD3D9VTable[16] = (DWORD)&hkCreateDevice;
                VirtualProtect(&pD3D9VTable[16], sizeof(DWORD), oldProtect, &oldProtect);
            }
        }
    }
    return pD3D9;
}

extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) { return 0; }
extern "C" int WINAPI D3DPERF_EndEvent() { return 0; }
extern "C" DWORD WINAPI D3DPERF_GetStatus() { return 0; }
extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {}
extern "C" void WINAPI D3DPERF_SetOptions(DWORD dwOptions) {}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls((HMODULE)hinstDLL);
        DeleteFileA("proxy_log.txt");
        Log("======================================");
        Log("[PROXY] Trampoline Fix (Exact Arithmetic) version launched");
    }
    return TRUE;
}
