# Anthology Performance v87 - DX11 Detail Instancing

Companion shader addon for the v87 DX11 and DX11-AVX engine binaries.

What it changes:

- uses real DX11 hardware instancing for level details and grass;
- uploads visible instance data once per frame and reuses it in shadow passes;
- preserves the active Screen Space Shaders 23.5 wind, interactive grass,
  terrain alignment, motion vectors and TAA jitter;
- keeps the existing density, draw radius, distance fade, LOD thresholds and
  pixel shader unchanged.

The engine grows its instance buffer before mapping on unusually dense levels,
so it does not drop grass when the original test patch's fixed capacity would
overflow.

This addon must be enabled above ScreenSpaceShaders Update 23.5 and must only be
used with the matching v87 engine. Disable it when rolling back to v86.
