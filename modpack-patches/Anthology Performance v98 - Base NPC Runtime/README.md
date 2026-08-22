# Anthology Performance v98 - Base NPC Runtime

Runtime companion for the v86-based base/NPC candidate. It keeps the accepted
Lua GC settings, working MT UI and functor cache, while disabling continuous
frame diagnostics during the performance test. Experimental Task Manager MT is
kept off.

No new game or cache purge is required. Disable this module to roll back only
the runtime commands.
