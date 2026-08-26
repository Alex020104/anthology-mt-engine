# Anthology PiP Rework v126

MO2 compatibility addon for the v126 PiP viewport build and the current
Anthology HARD mod stack. It is designed for the Anthology v125 engine baseline
and overrides the existing PiP MCM page, four active R3/R4 lens shaders, and the
two active Beef NVG/Heat Vision scripts with narrow PiP compatibility copies.

## What is active in v126

- Main-view TAA remains active while PiP is enabled and advances only on main
  frames. The lens uses stable, non-jittered SMAA so its sparse camera cannot
  contaminate main motion history or create cross-camera ghosting.
- The lens consumes the main-view exposure snapshot; PiP frames no longer
  advance global auto-exposure.
- Head NVG, head thermal, gas-mask and rain overlays are composed once in the
  main view. A weapon-defined thermal lens remains local to the PiP pass.
- The 2D scope night-vision PPE is no longer started merely because the weapon
  is hidden from a PiP world pass. This keeps hands and head NVG state stable.
- Fixed PiP-only blue/purple colour grading was removed from both active lens
  shader variants. Dirt, glass reflections and authored non-PiP lens colours
  remain intact.
- MCM exposes PiP sensitivity, Native/Quality/Balanced/Performance presets and
  explicit compatibility toggles for head NVG and thermal vision.
- A lightweight compatibility bridge reads the real `item_device` state, so
  Beef NVG/Heat Vision remain detectable during ADS. The bundled
  `z_beefs_nvgs.script` and `z_heatvision.script` are full-file, high-priority
  copies of the current HARD-profile winners; their only behavioural delta is
  the narrow allowed-PiP ADS branch. The original mod folders are not edited.
- A new scope is kept inactive until its first completed viewport capture;
  weapon changes can no longer display an old `$user$viewport2` frame.

## Presets

- Native: all effects currently supported by the PiP pass and the
  weapon-authored viewport cadence.
- Quality: full supported PiP lighting with SMAA, without PiP depth of field.
- Balanced: a three-frame minimum cadence and reduced PiP-only volumetrics,
  fog and sun shafts.
- Performance: the same conservative effect set as Balanced with a four-frame
  minimum cadence.

Native here means the complete supported PiP path, not a promise that every
main-view screen-space effect can be reused by a camera with different
matrices. SSFX passes that are unsafe for the second camera remain unchanged;
they are not sampled from stale main-view buffers.

The main renderer is unchanged by lower presets. Real reduced-resolution PiP
targets are deliberately deferred: the current pipeline copies a same-sized
swapchain resource, so target scaling requires a separate resolve/resample path
instead of an unsafe resource-size change.

## Required mod baseline

This package is a patch for the Beef NVG and Heat Vision versions currently
shipped by Anthology HARD. Keep those source mods enabled. Do not distribute or
install this addon as a generic standalone PiP package against different
versions of either script without rebasing the two compatibility copies.

## Installation and test

The folder is junctioned into the active MO2 profile as
`Anthology PiP Rework v126 - Viewport Parity`. Use the new v126 DX11 or
DX11-AVX executable, clear the shader cache once, and test an existing save.
No new game is required.

Test Native first, including ADS before/during/after head NVG and Heat Vision.
Then compare Balanced and Performance in the same scene. Apply MCM changes and
leave/re-enter ADS so the compatibility policy begins a clean scope session.
