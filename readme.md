# RetroPixel DirectX Wrapper

A lightweight, zero-overhead Direct3D 8 and Direct3D 9 wrapper designed to force **Nearest Neighbor (Point) filtering** in classic PC games. Say goodbye to blurry bilinear filtering and enjoy crisp, pixel-perfect retro graphics.

## 🚀 Features
* **True Pixel-Perfect Rendering:** Overrides hardware texture filtering to force `D3DTEXF_POINT`.
* **Zero Overhead:** Uses low-level Assembly Trampoline hooks. No performance drops, no stuttering.
* **Smart Scaling Fix:** Contains specific fixes for games like *Silent Hill 3* to prevent scaling bugs and screen zooming.
* **No Dependencies:** Written in pure C++ without heavy libraries like Detours or MinHook.

## 🎮 Confirmed Working Games
* *Need for Speed: Underground* (DX9)
* *Devil May Cry 3* (DX9)
* *Silent Hill 3* (DX8) - *Includes smart mipmap bypassing*

<details>
<summary>📸 Click to compare (Screenshots Before / After)</summary>
<p>

### Need for Speed: Underground
**Before (Bilinear):**</br>
![NFS Before](screenshots/nfsu_bilinear1.png)

**After (Point):**</br>
![NFS After](screenshots/nfsu_point1.png)

---

### Devil May Cry 3
**Before (Bilinear):**</br>
![DMC Before](screenshots/dmc_bilinear2.png)

**After (Point):**</br>
![DMC After](screenshots/dmc_point2.png)

---

### Silent Hill 3
**Before (Bilinear):**</br>
![SH3 Before](screenshots/sh3_bilinear1.png)

**After (Point):**</br>
![SH3 After](screenshots/sh3_point1.png)

</p>
</details>

👉 [More Screenshots](screenshots/)

## 🛠️ Installation
1. Go to the **[Releases](../../releases)** tab on the right side of this page.
2. Download the latest `.zip` archive.
3. Extract `d3d9.dll` (for DirectX 9 games) or `d3d8.dll` (for DirectX 8 games) into the folder where the game's main `.exe` is located.
4. Launch the game and enjoy crisp pixels! *A `proxy_log.txt` will be generated in the game folder to confirm the wrapper is active.*

## ⚙️ How it Works (Under the Hood)
Unlike standard wrappers that simply create a proxy VTable, this project uses **Inline Trampoline Hooking**. Many older games or third-party mods (like Widescreen Fixes) wrap the Direct3D device, bypassing standard VTable hooks. This wrapper directly patches the binary machine code of `d3d9.dll` and `d3d8.dll` in memory for functions like `SetTexture` and `SetSamplerState`, ensuring the filtering override cannot be bypassed by the game engine.

<details>
<summary>🔍 Let's take a closer look at the picky engines</summary>
<p>

### Need for Speed: Underground (EA EAGL engine)

**The Issue:** The game completely ignored standard proxy VTables. While hooks appeared to be successfully deployed, texture filtering in the game remained blurry and unaffected.

**Engine Behavior:** The EAGL engine interacts directly and aggressively with the GPU state machine. It utilizes an internal texture and state manager that overwrites `SetSamplerState` calls with its own hardcoded values during device initialization and screen resets, effectively wiping out conventional software hooks.

**Resolution:** To bypass the engine's internal state management, an **Inline Assembly Trampoline Hook** was implemented. The wrapper intercepts execution at the machine-code level inside the system `d3d9.dll` in memory. This ensures that regardless of the engine's internal overrides, execution is forcibly rerouted to enforce the pixel-perfect filtering parameters.

---

### Devil May Cry 3: Dante's Awakening (Capcom engine)

**The Issue:** Early wrapper implementations caused immediate application hangs, crashes, or segmentation faults during game initialization and scene transitions.

**Engine Behavior:** This PC port features a highly unstable rendering architecture that frequently breaks DirectX lifecycle standards. The engine dynamically destroys and recreates the Direct3D device context when loading levels and cutscenes. Furthermore, the engine performs low-level stack integrity checks. Standard function hooking alters the expected CPU register states, leading to stack corruption upon returning to the game code.

**Resolution:** A highly precise naked assembly hook was developed. The wrapper meticulously saves the state of all CPU registers (`EAX`, `ECX`, etc.) before execution, safely forces `D3DTEXF_POINT` filtering, and carefully restores the stack pointer to its exact original state. This allows the wrapper to remain completely transparent to the game's strict execution checks, resulting in rock-solid stability.

---

### Silent Hill 3 (Team Silent / Konami engine)

**The Issue:** This title runs on **DirectX 8** (requiring a custom `d3d8.dll` wrapper). Forcing global Nearest Neighbor filtering completely broke the game's rendering pipeline, causing extreme screen zooming, displaced UI coordinates, and severe texture shimmering.

**Engine Behavior:** The Team Silent engine uses a clever rendering trick to produce its signature atmospheric fog, noise, and motion blur effects. It dynamically generates low-resolution render targets and applies automatic mipmapping for post-processing layers. Enforcing a rigid point-filtering method globally forces the engine to apply pixelated scaling to post-processing buffers and UI elements, destroying the game's internal coordinate scaling.

**Resolution:** An **Intelligent State Filter** was built into the DirectX 8 wrapper. The hook intercepts every `SetTextureStageState` call and evaluates its context. If the engine attempts to configure texture states for UI rendering or specific fullscreen post-processing buffers, the wrapper steps aside and permits standard bilinear filtration. Point filtering is selectively enforced only when 3D world geometry and environmental textures are being processed, preserving both the pixel-perfect texture aesthetic and the correct screen geometry.

</p>
</details>

## 🤝 Credits
Developed by [alexey318 / User09] and inspired by the pursuit of preserving retro game aesthetics.
