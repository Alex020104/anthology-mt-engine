# Anthology Performance v77 - v66 Frame Pacing

Standalone runtime companion for the paired v77 engine build.

- Restores the user-accepted v66 LuaJIT collector cadence: step 76, up to 25
  calls and a 5000 microsecond render-overlap budget.
- Keeps Lua VM access serialized on GameThread after script MT callbacks. It
  does not run the single LuaJIT state concurrently from another worker.
- Disables the later adaptive/post-load skip profile which accumulated mark
  debt and produced measured 100-300 ms atomic collector stalls.
- Preserves every v76 renderer, LOD/HOM, marker and spawn-publication change.

There is no per-frame Lua callback and no save data. A new game and shader-cache
purge are not required.

Rollback: disable this addon and restore the paired v76 binaries and user.ltx
from `E:/ANTHOLOGY_BACKUPS/20260821_v77_pre_v66_pacing_npc_persistence`.
