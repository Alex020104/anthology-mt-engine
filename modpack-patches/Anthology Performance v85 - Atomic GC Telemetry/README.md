# Anthology Performance v85 - Atomic GC Telemetry

Runtime companion for the V85 engine candidate.

- Retains the accepted V84/V81 collector cadence: 76 KB steps, up to ten calls
  inside the existing 1200 us render-overlap allowance.
- Keeps the corrected V84 A-Life budget and does not alter renderer, loading,
  NPC placement, LOD/HOM, PiP, saves, or addon behaviour.
- Enables the existing coarse 300-frame profiler. The paired engine records
  only LuaJIT atomic phases lasting at least 10 ms; it performs no logging or
  allocation from inside the collector.
- Keeps per-item `seqParallel` timing disabled so the diagnostic build does not
  introduce the previously measured profiling overhead.

Expected log markers:

- `[anthology/v85] V84 FPS path with atomic phase telemetry active`
- `[Lua GC/v85] V84 cadence retained with atomic phase telemetry`
- `[Lua GC/xray-atomic]`

Rollback: disable this addon and restore the V84 binaries. No new game, save
migration, or shader-cache purge is required.
