# Anthology Performance v98 - Pools of Blood Base Grid

Standalone compatibility patch for Anthology's Pools of Blood script
(`FB9A6F08FAFBE5DE49B3CB750DD5F4E8A42E472C3CB2175415291C5477750D9D`).

The original callback scans every live blood-pool point for every online NPC
update and again every actor frame. On populated bases that cost grows as
`online NPCs x blood pools`.

This patch keeps pool creation, lifetime, visuals and bloody footsteps intact.
It builds a reusable 2 m spatial grid, checks only neighbouring cells, and
paces proximity checks to 50 ms for the actor and 75-105 ms per NPC. NPC
deadlines are staggered by object ID so a whole base does not poll on one frame.

No save data is added. Disable this MO2 module to roll back.
