# Anthology Performance v81 - Tactic Compass Scan Pacing

Standalone MO2 override for the active Tactic Compass stack.

- Keeps the accepted v76 HUD, marker and MCM caches.
- Enemy detection uses the paired engine's unsorted spatial scan and falls back
  to the original binding with older executables.
- Marker validation removes stale entries directly instead of allocating a
  temporary removal table for every ID.
- Marker categories, detection radii, waveform, corpse cleanup, appearance and
  save data are unchanged.

Rollback: disable this addon in MO2. The lower Tactic Compass providers are not
modified.
