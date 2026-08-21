# Anthology Performance v83 - Incremental Atomic GC

Runtime companion for the paired v83 engine build.

- Restores the stable v81 collector cadence: step 10, up to six calls and a
  1200 microsecond renderer-overlap budget.
- Keeps the collector running continuously, so every cycle reaches sweep and
  releases memory. There is no movement-dependent atomic deferral.
- The engine-side LuaJIT change drains one snapshot of repeated marking through
  normal incremental steps before the final atomic pass.
- Finalized userdata is moved out of the list scanned by every later atomic
  phase, while remaining subject to normal marking and sweeping.

No renderer, loading, A-Life/NPC placement, PiP, save data or gameplay rule is
changed. A new game and shader-cache purge are not required.
