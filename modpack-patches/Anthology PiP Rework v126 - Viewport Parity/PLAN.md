# Anthology PiP rework plan

Status: v126 stage 1 and controls test implementation; visual-parity work is
still measured in stages. The folder is the canonical addon
and is installed into MO2 through a directory junction.

## Objectives

1. Make the PiP lens match the active main DX11 renderer in lighting, tone,
   temporal stability, weather and post-processing while retaining the clean
   optical magnification and full-resolution weapon sight behavior.
2. Add PiP aiming sensitivity, real quality presets, and explicit 2D NVG and
   thermal compatibility controls to the existing PiP MCM page.
3. Keep hands and the weapon visible when NVG or thermal mode changes during
   ADS, and avoid leaking viewport-specific render state into the main pass.
4. Ship the result as a complete MO2 patch addon for the current Anthology HARD
   stack on D, with byte-exact backups on E, and build/test both regular DX11
   and DX11-AVX.

## Confirmed baseline defects addressed by v126

- `rak_pip_quality_mcm.script` exposed `render_quality`, and the engine console
  registered `scope_lense_render_quality`, but the renderer never read it. v126
  keeps that legacy value inert and uses a new `scope_lense_quality_preset`
  command for four renderer policies.
- `ActorInput.cpp` scaled mouse input only by PiP optical FOV. v126 applies a
  separate user multiplier after that optical correction.
- TAA jitter was disabled whenever the second viewport was active. v126 keeps
  main-view TAA active on main frames, preserves its motion history across lens
  captures, and gives the non-jittered PiP pass SMAA instead of unsafe temporal
  reprojection through shared object/grass histories.
- PiP activation had no global MCM policy for 2D NVG or heat vision, and a PiP
  world frame was misclassified as a 2D scope PPE condition. v126 separates
  those paths. A separate observer bridge reports their real `item_device`
  state. High-priority full-file copies of the current Beef/HeatVision winners
  add only the narrow allowed-PiP ADS branch; their original mod folders stay
  untouched and are explicit dependencies of this patch.
- A newly activated or changed PiP owner now waits for a completed capture
  before the lens samples `$user$viewport2`.

## Stage 1 - viewport-state correctness

- Keep second-viewport mode state explicit and atomically clear active, camera,
  thermal and thermal-mode state whenever the viewport is disabled.
- Keep the existing world-only PiP pass, but never use its intentional weapon
  culling as the trigger for 2D scope PPE or head-NVG lifecycle changes.
- Apply the 2D NVG/thermal overlay once, after PiP composition, unless a lens
  mode explicitly requests a viewport effect. Do not double-darken or double
  tint the scope image.
- Keep main temporal history and its object/grass/wind cadence untouched by the
  sparse PiP frames. PiP activation must not disable TAA in the main renderer;
  the lens itself uses non-jittered spatial AA.

Acceptance: toggling NVG/thermal before ADS, during ADS and after leaving ADS
never hides hands, changes the main camera FOV, or leaves stale post-process
state behind.

## Stage 2 - visual parity at the default preset

- Feed PiP the same active sun, fog, environment, exposure, tone mapping,
  shadows and compatible screen-space settings as the main view.
- Preserve independent camera matrices and lens magnification; do not copy the
  main camera transform or blur the magnified image to imitate 2D scopes.
- Invalidate stale lens content on zoom policy, weapon, resolution and device
  changes without disturbing the main TAA history.
- Audit texel-size and motion-vector constants against the actual PiP render
  target rather than the backbuffer.

Acceptance: fixed screenshots of the same scene/weather show matching colour,
lighting and material response outside optical magnification. No ghost from a
previous zoom step or previous weapon remains in the lens.

## Stage 3 - MCM controls

Extend the existing PiP page rather than creating a conflicting menu:

- `PiP aiming sensitivity`: float multiplier `0.25..2.00`, default `1.00`.
  Apply it after automatic optical-FOV scaling, so all zoom steps remain
  proportional.
- `PiP image quality`: replace the current no-op with four real presets:
  - Native: complete supported PiP pass and weapon-authored cadence; default.
  - Quality: full supported PiP lighting with SMAA and no PiP depth of field.
  - Balanced: conservative cadence and reduced PiP-only volumetrics/fog/shafts.
  - Performance: four-frame minimum cadence with the Balanced effect set.
- `Allow PiP with NVG`: on/off, default on.
- `Allow PiP with thermal`: on/off, default on.

When a compatibility toggle is off and its device is active, fall back cleanly
to the weapon's normal 2D/3DSS scope behavior for that ADS session. Do not
silently disable the device or mutate the saved weapon configuration.

## Stage 4 - performance implementation (next after v126 validation)

- Make quality tiers drive actual PiP render-target size and PiP-only effect
  selection. Recreate dependent resources safely on apply/device reset.
- Keep native/quality viewport rendering concurrent only where renderer data is
  immutable. GPU context submission, target switching and global renderer
  commits remain on the render thread.
- Cache stable lens resources and shader permutations. Invalidate on weapon,
  lens, renderer preset, screen resolution or shader-relevant MCM changes.
- Measure CPU submit time, GPU PiP pass time, VRAM, 1% low and missed-update
  cadence. Do not call a preset faster based only on average FPS.

## Stage 5 - compatibility and release matrix

Test both `AnomalyDX11.exe` and `AnomalyDX11AVX.exe` with:

- TAA on/off, dynamic zoom steps, weapon switching and save/load while scoped;
- SSS, SSAO, SSR, volumetrics, rain, emissions and day/night transitions;
- Beef NVG and HeatVision/thermal 2D overlays, each PiP compatibility toggle,
  and weapon-defined thermal scopes;
- hands enabled/disabled, HUD variants, 16:9 and ultrawide resolutions;
- Native, Quality, Balanced and Performance presets with frametime capture.

Release gates:

- no missing hands, black/stale lens, state leak, save incompatibility or MCM
  reset after restart;
- Native visual parity is the default and does not regress the main renderer;
- each lower preset has measured GPU/frametime benefit;
- complete addon lives under `D:/ANTHOLOGY_DEV/addons`, originals and replaced
  binaries live under a dated `E:/ANTHOLOGY_BACKUPS` directory, and the active
  MO2 profile receives only a directory junction to the D addon.
