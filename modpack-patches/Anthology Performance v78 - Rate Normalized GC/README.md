# Anthology Performance v78 - Rate Normalized GC

Runtime companion for the paired v78 engine build.

- Keeps the accepted v66 LuaJIT step of 76 and continuous post-load collection.
- Normalizes collector work by frame duration: approximately 600 incremental
  steps and 200 ms of permitted overlap work per second.
- A 100-FPS frame receives roughly six calls / 2 ms; a 40-FPS frame receives
  roughly fifteen calls / 5 ms. The collector therefore keeps its throughput
  without erasing the high-FPS renderer gain.
- A step is never started after render overlap has ended. Lua remains owned by
  GameThread after script callbacks; no unsafe concurrent VM access is added.

No save fields, visual settings or gameplay callbacks are changed. A new game
and shader-cache purge are not required.
