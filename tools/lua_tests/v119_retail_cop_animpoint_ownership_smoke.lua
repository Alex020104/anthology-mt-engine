local gamedata = assert(arg[1], "path to v119 gamedata is required")
local script_path = gamedata .. "\\scripts\\zzzzzzzzzzzzzz_anthology_cutscenes_v119_animpoint_ownership.script"
local staging_path = gamedata .. "\\scripts\\zzzzzzzzzzzz_anthology_cutscenes_v114_staging.script"

local script_file = assert(io.open(script_path, "rb"))
local script_source = script_file:read("*a")
script_file:close()
assert(not string.find(script_source, "set_position(", 1, true),
	"v119 must not teleport NPCs")
assert(not string.find(script_source, "RegisterScriptCallback", 1, true),
	"v119 must not add a per-frame callback")
assert(not string.find(script_source, "state_mgr.set_state", 1, true),
	"v119 must not force a state-manager transition")
assert(not string.find(script_source, "function class:initialize", 1, true),
	"v119 must not override action initialize")
assert(not string.find(script_source, "function class:finalize", 1, true),
	"v119 must not override action finalize")

local staging_file = assert(io.open(staging_path, "rb"))
local staging_source = staging_file:read("*a")
staging_file:close()
assert(string.find(staging_source, "if not patrol then", 1, true),
	"v119 retained the invalid native-function patrol guard")
assert(not string.find(staging_source, "type(patrol) ~= \"function\"", 1, true),
	"v119 still rejects the live callable luabind patrol object")

local current_level = "pripyat"
level = {
	name = function()
		return current_level
	end,
}

local log_lines = {}
function printf(pattern, ...)
	table.insert(log_lines, string.format(pattern, ...))
end

function CreateTimeEvent()
	error("xr_animpoint must be available without a retry in this smoke test")
end

db = { storage = {} }

local original_calls = 0
xr_animpoint = {
	animpoint = {
		position_riched = function(self)
			original_calls = original_calls + 1
			return self.base_result == true
		end,
	},
}

local function make_npc(id, section)
	local npc = {
		id = function()
			return id
		end,
		name = function()
			return section .. "_object"
		end,
		section = function()
			return section
		end,
	}
	db.storage[id] = { object = npc }
	return npc
end

local function make_controller(id, cover, action, base_result)
	return {
		npc_id = id,
		st = { cover_name = cover },
		current_action = action,
		base_result = base_result,
	}
end

assert(loadfile(script_path))()
on_game_start()
local wrapped = xr_animpoint.animpoint.position_riched
on_game_start()
assert(xr_animpoint.animpoint.position_riched == wrapped,
	"v119 wrapped position_riched twice")

local pri_entries = {
	{ "pri_a15_actor", "pri_a15_animpoint_actor" },
	{ "pri_a15_vano", "pri_a15_animpoint_vano" },
	{ "pri_a15_sokolov_scene", "pri_a15_animpoint_sokolov" },
	{ "pri_a15_zulus", "pri_a15_animpoint_zulus" },
	{ "pri_a15_wanderer", "pri_a15_animpoint_wanderer" },
	{ "pri_a15_military_tarasov", "pri_a15_animpoint_military_1" },
	{ "pri_a15_military_2", "pri_a15_animpoint_military_2" },
	{ "pri_a15_military_3", "pri_a15_animpoint_military_3" },
	{ "pri_a15_military_4", "pri_a15_animpoint_military_4" },
}

current_level = "pripyat"
for index, entry in ipairs(pri_entries) do
	local id = 100 + index
	make_npc(id, entry[1])
	local controller = make_controller(id, entry[2], "pri_a15_camera_motion_" .. tostring(index), false)
	local before = original_calls
	assert(wrapped(controller) == true, "PRI ownership was not retained for " .. entry[1])
	assert(original_calls == before, "PRI ownership delegated to the Anomaly distance guard")
end

local jup_entries = {
	{
		section = "jup_b219_actor",
		cover = "jup_b219_actor_smart_cover",
		actions = { "pri_a15_idle_none", "jup_b219_actor_all" },
	},
	{
		section = "jup_b219_stalker_tech",
		cover = "jup_b219_azot_smart_cover",
		actions = { "pri_a15_idle_strap", "jup_b219_azot_all" },
	},
}

current_level = "jupiter"
for index, entry in ipairs(jup_entries) do
	local id = 200 + index
	make_npc(id, entry.section)
	for _, action in ipairs(entry.actions) do
		local controller = make_controller(id, entry.cover, action, false)
		local before = original_calls
		assert(wrapped(controller) == true,
			"JUP ownership was not retained for " .. entry.section .. "/" .. action)
		assert(original_calls == before,
			"JUP ownership delegated to the Anomaly distance guard")
	end
end

-- Once current_action is cleared, initial reaching must return to the base
-- Anomaly implementation and may legitimately report that the point is lost.
current_level = "pripyat"
make_npc(300, "pri_a15_actor")
local stopped = make_controller(300, "pri_a15_animpoint_actor", nil, false)
local before_stopped = original_calls
assert(wrapped(stopped) == false, "stopped animpoint was incorrectly retained")
assert(original_calls == before_stopped + 1, "stopped animpoint did not delegate")

-- Before an action starts the NPC must still physically reach the authored
-- smart-cover through the untouched base evaluator. v119 is not allowed to
-- claim the point early or skip action_reach_animpoint.
make_npc(301, "pri_a15_actor")
local approaching = make_controller(301, "pri_a15_animpoint_actor", nil, false)
local before_approaching = original_calls
assert(wrapped(approaching) == false, "v119 skipped physical initial reaching")
assert(original_calls == before_approaching + 1, "initial reaching did not delegate")

-- The same base path is restored after scene finalization clears the action.
approaching.current_action = "pri_a15_actor_all"
assert(wrapped(approaching) == true, "active root-motion lost scene ownership")
approaching.current_action = nil
local before_finalized = original_calls
assert(wrapped(approaching) == false, "v119 blocked physical reaching after finalize")
assert(original_calls == before_finalized + 1, "post-finalize reaching did not delegate")

local delegate_cases = {
	{ level = "zaton", section = "pri_a15_actor", cover = "pri_a15_animpoint_actor", action = "pri_a15_actor_all", base_result = true },
	{ level = "pripyat", section = "sim_default_stalker_1", cover = "pri_a15_animpoint_actor", action = "pri_a15_actor_all" },
	{ level = "pripyat", section = "pri_a15_actor", cover = "wrong_cover", action = "pri_a15_actor_all" },
	{ level = "pripyat", section = "pri_a15_actor", cover = "pri_a15_animpoint_actor", action = "sit_ass" },
	{ level = "jupiter", section = "jup_b219_zulus", cover = "jup_b219_zulus_smart_cover", action = "jup_b219_zulus_all" },
	{ level = "jupiter_underground", section = "pas_b400_zulus", cover = "pas_b400_cover", action = "pas_b400_zulus_all" },
	{ level = "jupiter_underground", section = "pas_b400_vano", cover = "pas_b400_cover", action = "pas_b400_vano_all" },
	{ level = "jupiter_underground", section = "pas_b400_sokolov", cover = "pas_b400_cover", action = "pas_b400_sokolov_all" },
	{ level = "jupiter_underground", section = "pas_b400_wanderer", cover = "pas_b400_cover", action = "pas_b400_wanderer_all" },
}

for index, case in ipairs(delegate_cases) do
	local id = 400 + index
	current_level = case.level
	make_npc(id, case.section)
	local controller = make_controller(id, case.cover, case.action, case.base_result == true)
	local before = original_calls
	assert(wrapped(controller) == (case.base_result == true),
		"out-of-scope result changed at index " .. tostring(index))
	assert(original_calls == before + 1, "out-of-scope case did not delegate at index " .. tostring(index))
end

-- Simulate a long root-motion chain that moves far outside the smart cover.
-- The retail invariant must keep the same action alive for every evaluator tick,
-- so no reach action/finalize/re-anchor cycle can be scheduled.
current_level = "pripyat"
make_npc(500, "pri_a15_zulus")
local moving = make_controller(500, "pri_a15_animpoint_zulus", "pri_a15_zulus_all", false)
local finalize_count = 0
local reach_action_count = 0
local anchor_epochs = 1
local before_motion = original_calls
for _ = 1, 200 do
	if not wrapped(moving) then
		finalize_count = finalize_count + 1
		reach_action_count = reach_action_count + 1
		anchor_epochs = anchor_epochs + 1
	end
end
assert(original_calls == before_motion, "root-motion chain hit the Anomaly distance guard")
assert(finalize_count == 0, "root-motion finalized the active CoP animpoint")
assert(reach_action_count == 0, "root-motion scheduled a walk back to the smart cover")
assert(anchor_epochs == 1, "root-motion created a second absolute anchor epoch")

local saw_active = false
local saw_zulus = false
for _, line in ipairs(log_lines) do
	if string.find(line, "retail CoP animpoint ownership active", 1, true) then
		saw_active = true
	end
	if string.find(line, "npc=pri_a15_zulus_object", 1, true)
		and string.find(line, "action=pri_a15_zulus_all", 1, true)
	then
		saw_zulus = true
	end
end
assert(saw_active, "v119 activation was not logged")
assert(saw_zulus, "v119 retained action was not logged")

print("v119 retail CoP animpoint ownership smoke test passed")
