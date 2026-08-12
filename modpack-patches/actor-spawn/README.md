# Anthology Performance - Actor Spawn

Installed standalone addon:
`D:/ANTHOLOGY_DEV/addons/Anthology Performance - Actor Spawn`.

The addon preserves Anthology's actor-spawn behavior but replaces the Lua loop
over all 65,534 possible ALife IDs with the paired engine export
`iterate_objects_by_clsid`. Filtering happens in the native ALife registry and
Lua receives only actual phantom objects. The original modpack provider is not
edited.
