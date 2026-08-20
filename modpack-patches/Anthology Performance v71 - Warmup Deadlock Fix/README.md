# Anthology Performance v71 - Warmup Deadlock Fix

Hotfix profile paired with the v71 DX11 and DX11-AVX engine binaries. It keeps
the accepted v70 runtime tuning but corrects the covered world-warmup completion
rule in the engine.

The v70 implementation required all client queues to be empty on every warmup
frame. Normal gameplay continuously produces runtime events after the world has
started, so the load session could remain active forever. That also kept the
loading screen visible and input blocked at `Zone awaits`.

v71 still requires a ready level, player control and drained load queues before
starting the covered warmup. Once it has started, later events are treated as
normal runtime work and cannot prevent the fixed five-second phase from ending.
The `Zone awaits` key prompt is now deferred until that phase has actually
finished, so it is never displayed while input is intentionally locked.

No addon script, UI file, renderer setting or save data is changed. No new game
or shader-cache purge is required. The exact pre-v71 engine and log are stored
in `E:/ANTHOLOGY_BACKUPS/20260821_v71_pre_warmup_deadlock_fix`.
