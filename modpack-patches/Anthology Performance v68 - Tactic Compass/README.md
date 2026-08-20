# Anthology Performance v68 - Tactic Compass

Standalone override for Tactic Compass in Anthology 2.1.

- Compass direction, marker positions and waveform animation still update on
  every rendered frame; this patch does not impose the old 30 Hz cap.
- MCM-derived layout and marker definitions are cached for 250 ms.
- Saved marker state avoids the duplicate 65,534-ID catalogue scan on reload.
- Stale map-spot validation is spread across frames, and enemy radius scans are
  prevented from landing together on one frame.

A new game or legacy save without compass marker state performs the original
catalogue scan once. Rollback by disabling only this MO2 addon.
