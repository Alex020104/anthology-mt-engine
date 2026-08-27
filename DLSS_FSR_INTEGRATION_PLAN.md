# Anthology DLSS / FSR integration plan

## Current implementation status (v134, 2026-08-28)

- The renderer-scale foundation and the first auditable FSR 3.1.2/DLSS Super
  Resolution paths are implemented together on
  `anthology-v134-upscale-foundation` for live testing.
- `r4_upscaler off` remains the engine default and preserves the native v133
  path. The owner's installed RTX 5070 test profile currently selects DLSS
  Quality; unsupported hardware, missing runtime DLLs, MSAA, or missing SSS
  motion-vector shaders fall back to native rendering.
- World color/depth/motion resources render at the selected internal size. HUD,
  menus, PDA, loading UI and the final backbuffer remain display-sized.
- The main camera owns the temporal jitter/history. The old SSS TAA resolve is
  skipped while a vendor upscaler is active, PiP/SVP frames use only a spatial
  present and never advance main history, and load precache keeps history in a
  reset state until normal gameplay resumes.
- FSR frame generation is deliberately not enabled. Reactive masks, additional
  camera-cut resets, runtime MCM controls and broader GPU/HDR validation remain
  stabilization work for the following versions.

## Source audit (2026-08-27)

- `SSS UPDATE 24 - ALPHA7 (1)` ships two replacement engine binaries, NVIDIA
  Streamline/DLSS DLLs and a small shader/terrain payload. It does not ship the
  matching engine source or standalone FSR runtime files, so its EXEs cannot be
  merged safely with Anthology's loading, PiP, A-Life and renderer changes.
- The local repository history contains an auditable implementation chain:
  - `e729e443df`: render scaling and FSR2 foundation;
  - `0f79e73160`: direct NVIDIA NGX/DLSS integration;
  - `5b1b7a319c`: depth-upscale support;
  - `b11fee6be`: FSR2 replacement with FSR 3.1.2.
- These commits are IX-Ray-side architecture, not drop-in patches for the
  current Monolith-derived renderer. Code is to be adapted subsystem by
  subsystem; no foreign EXE will replace the Anthology engine.

## Required invariants

1. `vid_scale = 1.0` and upscaler `Off` must preserve the current native path.
2. Unsupported hardware or a missing vendor DLL must fall back to native
   rendering with one clear log message, never an assertion or startup crash.
3. Main-view temporal history, SSS history and PiP/SVP history must remain
   separate. A PiP capture must never advance or reset the main upscaler.
4. HUD, MCM, PDA and loading UI are composed at display resolution after the
   world upscaler. Text must not be temporally reconstructed.
5. Initial delivery covers spatial/temporal upscaling only. Frame generation is
   a later opt-in stage after frame pacing and input latency are validated.
6. The shader cache is not managed, deleted or rewritten by the integration.

## v134: renderer-scale foundation

- Introduce distinct display and world-render dimensions instead of reusing
  `Device.dwWidth/dwHeight` everywhere.
- Allocate the world G-buffer, color, depth and motion-vector resources at the
  selected render size while retaining display-sized output and UI targets.
- Port the IX-Ray depth-upscale stage and normalize motion-vector units,
  direction and jitter for the current SSS/TAA shaders.
- Add native, 77%, 67%, 59% and 50% presets plus a custom scale. Resolution
  changes recreate only the required render resources and reset temporal state
  once.
- Validate native mode first; it is the rollback/reference path for every next
  stage.

## v135: FSR 3.1.2 stabilization

- Stabilize the adapted FSR 3.1.2 wrapper and DX11 device lifetime after live
  v134 testing.
- Feed low-resolution HDR color, linear depth and motion vectors; add reactive
  and transparency/composition masks for particles, weapon glass and water.
- Reset the context on load, teleport, camera effector discontinuity,
  resolution/preset change and device reset.
- Keep frame generation disabled until upscaling is stable and frame pacing is
  measured independently.

## v136: DLSS stabilization

- Continue from the auditable direct-NGX path (`0f79e73160`) already adapted in
  v134 rather than loading Alpha7's opaque replacement EXE. Finalize the
  wrapper/API after the shared render inputs are verified with FSR.
- Probe availability and driver requirements at runtime; expose only supported
  modes. Package `nvngx_dlss.dll` beside the engine without making it a hard
  dependency for AMD/Intel users.
- Compare direct NGX with the Alpha7 Streamline layout. Streamline is adopted
  only if its interposer lifecycle is proven compatible with ReShade, overlays
  and the current launcher.

## PiP policy under main-view upscaling

- PiP keeps its full-rate alternating capture and its own 25-100% spatial
  quality control from v133.
- The main temporal upscaler is not evaluated on SVP frames and never samples
  SVP depth/motion history.
- First integration composites a completed PiP texture into the upscaled main
  frame. A second DLSS/FSR context for the lens is explicitly out of scope until
  the native/spatial PiP path is stable.

## Acceptance matrix

- DX11 and DX11-AVX; NVIDIA RTX, AMD RDNA and unsupported-GPU fallback.
- 1080p, 1440p and 4K; fullscreen and borderless; HDR off/on where available.
- Static scene, rapid camera rotation, grass/foliage, particles, water,
  interiors, loading/quickload, camera effectors and cutscenes.
- PiP normal/NVG/thermal, rapid ADS switching and all quality values.
- Native-image regression, GPU frame time, 1%/0.1% lows, input latency,
  temporal ghosting and VRAM usage logged separately.

The v133 branch contains only the SSS restore and corrected full-rate PiP
quality behavior. No DLSS/FSR DLL is installed by v133.
