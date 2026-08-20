# Anthology Performance v68 - Interactive PDA

Standalone override for Interactive PDA in Anthology 2.1.

- Removes an invisible synthetic workload that grew and saved a table up to
  65,534 entries. An old payload is discarded on the next save.
- Builds and persists the task-giver catalogue from actual simulation squads,
  then maintains it from NPC spawn callbacks.
- Reuses that catalogue for remote trading instead of another global ID scan.
- Uses `SIMBOARD.squads` for raid and status searches and bounds the emission
  sender selection loop.
- Staggers task/cooldown processing after load and removes the permanent no-op
  callback paired with the deleted synthetic workload.

Existing saves are supported. Rollback by disabling only this MO2 addon.
