# Anthology A-Life v80 - CoP Online Placement

Restores the original Call of Pripyat online-placement order around the active
Anthology NPC binder without replacing its footsteps, Exo or gameplay logic.

- Captures an NPC's saved offline level vertex before the modpack smart-terrain
  extension can clear `db.offline_objects`.
- Restores a unique saved vertex immediately during `net_spawn`, before the
  first rendered gameplay frame.
- Detects an already collapsed squad-centre batch and redirects those members
  to their assigned smart-job vertices. Squads still arriving at a smart are
  not teleported to a job.
- Uses the paired engine's stationary-member persistence for later switches.

There is no `actor_on_update`, recurring NPC scan, wider A-Life distance or new
save field. The v78 Lua placement addon must be disabled; its paired engine
position-preservation code remains active. Existing saves are supported and a
new game is not required.
