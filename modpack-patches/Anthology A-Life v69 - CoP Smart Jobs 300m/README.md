# Anthology A-Life v69 - CoP Smart Jobs 300m

Standalone replacement for `Anthology A-Life - Persistent 300m Groups`.

- Keeps the approximately 297 m online / 363 m offline hysteresis profile.
- Reapplies each already initialized smart-terrain job vertex whenever its NPC
  returns online. NPCs therefore appear at their authored camp, guard, patrol
  and work points instead of first appearing together at the squad centre.
- Uses the stock `db.spawned_vertex_by_id` net-spawn redirect used by the
  Call of Pripyat smart-job path. No new persistent save data is introduced.
- Has no `actor_on_update` callback and no recurring object scan.

Existing saves are supported. To retest an already visible group, move it
offline and return, or leave and re-enter the level. A new game is not needed.

Rollback: disable this addon, re-enable the previous 300 m addon and restore
the paired v68 engine binaries from the v69 backup.
