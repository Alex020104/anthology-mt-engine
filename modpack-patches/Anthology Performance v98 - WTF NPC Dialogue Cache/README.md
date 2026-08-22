# Anthology Performance v98 - WTF NPC Dialogue Cache

Standalone compatibility patch for Anthology WTF 4.2's `igi_task_manager.script`
(`A89726305C2C95EDB7A69D826EC345AD61019133D3EDE501775E22C91F0FEA76`).

Several dialogue predicates can request the same generated quest list for the
same NPC during one interaction. WTF then repeats task-cache construction,
macro validation and precondition checks. This patch reuses the result for only
350 ms. The next conversation or later predicate evaluates current conditions
normally, and no cache enters the save.

Disable this MO2 module to roll back.
