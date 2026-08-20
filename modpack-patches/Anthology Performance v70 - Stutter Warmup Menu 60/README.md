# Anthology Performance v70 - Stutter, World Warmup and Menu 60

Runtime profile paired with the v70 engine build. The active copy is applied
directly to `appdata/user.ltx`; this folder records the exact reversible values
without overriding any unrelated mod, script, renderer or PiP setting.

- `r__hom_dynamic off` disables the experimental per-child-visual HOM pass.
  The renderer's existing once-per-renderable spatial HOM rejection remains.
- Lua GC uses a 20-unit step, at most eight calls and a 1000-us overlap budget.
  This targets the measured 30-46 ms GC tails without moving LuaJIT onto a
  concurrently mutating thread.
- `load_world_warmup_ms 5000` keeps normal world updates and rendering behind
  the loading screen for five seconds after legacy precache and queue drain.
  Input remains blocked until the warm-up and final resource flush complete.
- `r__menu_framelimit 60` limits only an idle main/pause menu. Active loading
  and gameplay remain uncapped because `r__framelimit` stays at zero.

No new game or shader-cache purge is required. The exact pre-v70 engine,
symbols, log and runtime settings are in the paired backup on drive E.
