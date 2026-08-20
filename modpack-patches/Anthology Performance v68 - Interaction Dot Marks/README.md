# Anthology Performance v68 - Interaction Dot Marks

Standalone override for Interaction Dot Marks in Anthology 2.1.

- Advances the movement reference position instead of treating every frame
  after the first movement as another movement event.
- Keeps targeting responsive while limiting radius scans to useful cadences.
- Runs the large predictive scan only while travelling and staggers scanner
  startup to avoid a single post-load burst.

No save fields are added. Rollback by disabling only this MO2 addon.
