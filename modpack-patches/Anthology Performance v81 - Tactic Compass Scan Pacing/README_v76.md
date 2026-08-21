# Anthology Performance v76 - Tactic Compass GC

Standalone patch for Tactic Compass that keeps the 30 Hz HUD update rate and
visual behaviour while reducing Lua allocation pressure.

- Reuses compass, marker and waveform vectors/texture rectangles.
- Caches marker keys instead of concatenating them every refresh.
- Reuses the marker configuration table.
- Replaces per-scan closures and temporary seen tables with a generation map.
- Calculates waveform tint, blend and fade with numeric values rather than
  allocating several color tables every refresh.
