# Anthology Performance v86 - Leaf Luabind GC

Runtime companion for the V86 engine candidate.

- Keeps the accepted V84/V81 FPS and loading path unchanged: 76 KB Lua GC
  steps, up to ten calls inside the existing 1200 us overlap allowance.
- Removes provably leaf borrowed C++ luabind wrappers from LuaJIT's global
  atomic userdata-finalizer scan. Their native-only cleanup is performed by
  incremental sweep instead of producing a 45-60 ms stop every few seconds.
- A wrapper automatically returns to the stock finalizer path before it gains
  Lua fields, dependencies, ownership, or a different metatable.
- Keeps V84 A-Life pacing and NPC placement. Renderer, loading, LOD/HOM, PiP,
  saves, and addon behaviour are not changed by this patch.
- Keeps low-overhead atomic telemetry enabled. Any remaining atomic pause over
  10 ms includes cumulative leaf marked/unmarked/finalized counters in the log.

Expected log markers:

- `[anthology/v86] leaf luabind GC active; V84 FPS/load/A-Life path retained`
- `[Lua GC/v86]`
- `[Lua GC/xray-atomic]` only when an atomic phase still exceeds 10 ms

No new game, save migration, or shader-cache purge is required.
