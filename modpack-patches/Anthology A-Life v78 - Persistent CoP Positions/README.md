# Anthology A-Life v78 - Persistent CoP Positions

Standalone correction for NPCs that return online at one smart-terrain centre.

- Leaves the active A-Life online/offline distance unchanged. The former v69
  and v77 300 m modules must be disabled, so extra online NPCs do not consume
  the v76 FPS gain.
- Preserves an NPC's exact `db.offline_objects` vertex when it is unique.
- When several members of the same smart have the same collapsed offline
  vertex, repairs only that duplicate batch to the already assigned CoP job
  vertices before the online binder consumes the position.
- The paired engine preserves stationary offline member positions after this
  one-time repair. Moving squads keep the stock safe relocation path.

There is no `actor_on_update`, recurring scan or new save field. Existing saves
are supported; a collapsed group is repaired the next time it returns online.
