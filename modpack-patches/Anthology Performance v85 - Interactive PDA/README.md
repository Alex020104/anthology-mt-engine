# Anthology Performance v85 - Interactive PDA

Standalone first-use warm-up for the Interactive PDA UI.

- Leaves the original mod and the existing v68/v81 overrides untouched.
- Builds and caches `CustomPDAX(true)` during `actor_on_first_update`, while the
  level is still completing its first-update/loading phase.
- Moves the one-time monolithic XML/control construction out of the first PDA
  key press. It does not change menus, callbacks, tasks, save data, or layout.
- Logs the measured warm-up duration as `[anthology/v85/pda]`.

Expected trade-off: the first level load can become roughly 0.6 seconds longer,
but the first in-world PDA opening should no longer stop rendering for that work.

Rollback: disable this addon. No save migration is required.
