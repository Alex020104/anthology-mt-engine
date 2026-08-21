# Anthology Performance v82 - Lua GC Frame Pacing

Runtime companion for the paired v82 engine build.

- Starts an incremental Lua collection only after the heap reaches the normal
  pause threshold instead of forcing complete cycles continuously.
- Performs propagation in the existing serialized renderer-overlap worker.
- Stops immediately before LuaJIT atomic work while the player or camera moves.
- Admits atomic after 350 ms of visual idle, with a 120-second safety ceiling.
- Keeps automatic allocation-triggered GC out of FrameMove while the engine
  controller owns collection.

The corrected pre-atomic stop leaves LuaJIT in propagation state. It does not
repeat the rejected v60 behaviour that left the VM in atomic state and forced
JIT trace exits.

No renderer, loading, NPC placement, PiP, save field or gameplay rule changes.
A new game and shader-cache purge are not required.
