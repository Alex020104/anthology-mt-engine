# Anthology Performance v81 - Interactive PDA Compact Catalogue

Standalone MO2 override for Interactive PDA.

- Replaces the v76 legacy-save probe of all 65,534 possible ALife IDs with a
  bounded walk over real `SIMBOARD.squads` entries.
- Processes at most four squads / one millisecond every 100 ms, then persists
  the completed catalogue through the existing save fields.
- Online NPCs continue to be classified by `npc_on_net_spawn`.
- Removes the permanent no-op `manage_pda_x_on_update` callback; the paired GUI
  patch already disables its hidden synthetic workload.
- PDA task selection, cooldowns, banter and save compatibility are preserved.

Rollback: disable this addon in MO2. The lower Interactive PDA providers are
not modified.
