# Anthology A-Life v88 - 150m Budgeted Simulation

Companion configuration for the v88 DX11 and DX11-AVX engine binaries.

- Restores the intended 150 m steady A-Life radius instead of the active
  modpack's permanent 750 m radius. Objects outside that radius remain in
  offline simulation rather than running full online AI and rendering around
  populated bases.
- Keeps the old script-driven 2000 m switch disabled. The engine already
  enumerates server IDs and prepares their unique models and texture
  dependencies during the covered load, without bringing every NPC online.
- The matching engine divides the existing 0.9 ms A-Life budget between the
  online/offline switch scan and scheduled offline simulation. Up to the
  configured 20 objects may still be processed when they are cheap, but an
  expensive object stops the batch instead of pulling the rest into the same
  frame.
- Detailed profiling (`mt_frame_profile 1` and
  `mt_frame_profile_detail 1`) reports the slowest scheduled A-Life object
  every 300 frames without changing normal gameplay logging.

Existing saves are supported and no new game is required. This addon does not
replace scripts or alter NPC positions, smart jobs, save fields, renderer,
grass, loading code, Lua GC or PiP.
