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

What the problem was: The game flat-out ignored standard proxy DLLs that merely replaced the VTable. Logs showed hooks appeared to be installed, but textures remained blurry.

Engine quirk: EA’s EAGL engine works very aggressively and directly with GPU state. It has a built-in manager for textures and graphics states. On device initialization or Reset the game forcefully overwrites SetSamplerState with its own values, completely wiping out any standard hooks.

How I defeated it: Ordinary pointer swapping is powerless here. I used an aggressive inline assembly trampoline. Our code is injected directly into the “body” of the original system d3d9.dll in RAM and intercepts control at the CPU instruction level. Wherever the EA engine calls, it cannot physically bypass our trampoline.

---

### Devil May Cry 3: Dante's Awakening (Capcom engine)
What the problem was: When trying to run the game with early versions of the wrapper, it either froze hard during initialization or crashed to a blue screen/exit without explanation.

Engine quirk: This PC port from Capcom is infamous among bad ports. The engine violates DirectX conventions. Instead of creating one stable device and using it, the game can dynamically destroy the old device and create a new one while loading levels and cutscenes. Moreover, the game tries to control the integrity of certain libraries. When a standard hook called the original function, CPU registers got confused, the stack broke, and the game crashed.

How I defeated it: I created a “jewel” trampoline. Our assembly code doesn’t just redirect the call — it meticulously preserves the state of all CPU registers (EAX, ECX, etc.), substitutes filtering with D3DTEXF_POINT, and carefully restores the stack. For the picky DMC3 engine it appears as if the native system executed, so the game runs stably with no lag.

---

### Silent Hill 3 (Team Silent / Konami engine)

What the problem was: The trickiest opponent. Besides being a DirectX 8-era game (requiring a separate d3d8.dll wrapper), forcing nearest-neighbor broke all graphics: the screen wildly zoomed, the UI shifted, and textures started flickering and doubling.

Engine quirk: Team Silent used a clever trick to create the game’s oppressive atmosphere (fog, noise, and blur effects). The engine uses very low-resolution textures for post-processing and mipmap generation. When I forced everything to render with hard pixels (point filtering), the engine went haywire: it applied pixel filtering to UI and screen-effect textures, causing geometric collapse and scale distortion (a scaling bug).

How I defeated it: I taught our wrapper to “think.” In the DX8 code I implemented an intelligent call filter. The wrapper checks the parameters of each SetTextureStageState call. If it detects the game is setting up a UI texture or a specific fullscreen effect, the wrapper gracefully yields and allows the standard filtering. But as soon as it’s a 3D game texture (walls, monsters, roads), our hard pixel-perfect filter is applied.

</p>
</details>

## 🤝 Credits
Developed by [alexey318] / [User09] and inspired by the pursuit of preserving retro game aesthetics.
