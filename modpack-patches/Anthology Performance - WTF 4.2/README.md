# Anthology Performance - WTF 4.2

Standalone override for the exact WTF 4.2.2 script shipped by Anthology 2.1.

The original `quest_status` performs action, subtask, callback and map-target
processing every time Anomaly polls an active task, normally once per rendered
frame. This patch preserves the original task/save data and terminal-state
semantics but staggers normal processing over a 75-125 ms interval per task.
Completion and failure already present in the cache are returned immediately.

Original SHA-256:
`3F858C795226A50BE51204E001C13D242A9980EDFD499175C82CA663FCB09274`.

Rollback: disable only this MO2 mod. Existing saves do not require migration.
