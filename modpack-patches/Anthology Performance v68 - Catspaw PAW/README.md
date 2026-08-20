# Anthology Performance v68 - Catspaw PAW

Standalone override for the Catspaw files shipped with Anthology 2.1.

- Replaces the RF receiver's 65,534-ID scan every five seconds with iteration
  over actual online binders plus explicitly tracked RF targets.
- Skips temporary-pin table walks until the earliest pin can expire.
- Staggers PAW maintenance away from the former five-second collision point.

No saved fields or visible update rates are changed. Rollback by disabling only
this MO2 addon.
