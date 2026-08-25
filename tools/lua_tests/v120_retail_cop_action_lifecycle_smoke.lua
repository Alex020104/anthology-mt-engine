local gamedata = assert(arg[1], "path to v120 gamedata is required")
local script_path = gamedata .. "\\scripts\\zzzzzzzzzzzzzzz_anthology_cutscenes_v120_action_lifecycle.script"

local source_file = assert(io.open(script_path, "rb"))
local source = source_file:read("*a")
source_file:close()
assert(not string.find(source, "set_position(", 1, true), "v120 must not teleport NPCs")
assert(not string.find(source, "RegisterScriptCallback", 1, true), "v120 must not add a per-frame callback")
assert(not string.find(source, "create_anim_mov_ctrl", 1, true), "v120 must not replace the movement controller")

local current_level = "pripyat"
level = { name = function() return current_level end }
game_object = { level_path = 77 }
db = { actor = {}, storage = {} }

local log_lines = {}
function printf(pattern, ...)
	table.insert(log_lines, string.format(pattern, ...))
end
function CreateTimeEvent()
	error("all v120 target classes must exist during this smoke test")
end

local base_initialize_calls = 0
local base_execute_calls = 0
action_base = {
	initialize = function() base_initialize_calls = base_initialize_calls + 1 end,
	execute = function() base_execute_calls = base_execute_calls + 1 end,
}

local state_calls = {}
state_mgr = {
	set_state = function(npc, state, a, b, extra)
		table.insert(state_calls, { npc = npc, state = state, extra = extra })
	end,
}
xr_logic = {
	pick_section_from_condlist = function() return "walk" end,
}

local original_initialize_calls = 0
local original_finalize_calls = 0
local original_reach_calls = 0
local original_motion_calls = 0

xr_animpoint = {
	action_animpoint = {
		initialize = function() original_initialize_calls = original_initialize_calls + 1 end,
		finalize = function() original_finalize_calls = original_finalize_calls + 1 return "finalized" end,
	},
	action_reach_animpoint = {
		execute = function() original_reach_calls = original_reach_calls + 1 return "delegated" end,
	},
}

state_mgr_animation = {
	animation = {
		add_anim = function(self, animation_name)
			original_motion_calls = original_motion_calls + 1
			self.npc.animations = self.npc.animations + 1
			return "motion:" .. animation_name
		end,
	},
}

local function make_npc(id, section)
	local npc = {
		animations = 0,
		position_value = { distance_to_sqr = function() return 1 end },
		id = function() return id end,
		name = function() return section .. "_object" end,
		section = function() return section end,
		animation_count = function(self) return self.animations end,
		set_dest_level_vertex_id = function(self, value) self.dest_vertex = value end,
		set_desired_direction = function(self, value) self.desired_direction = value end,
		set_path_type = function(self, value) self.path_type = value end,
		position = function(self) return self.position_value end,
	}
	db.storage[id] = { object = npc }
	return npc
end

local function make_action(npc, cover)
	local point = {
		current_action = "pri_a15_actor_all",
		position_vertex = 123,
		vertex_position = {},
		smart_direction = { x = 1, y = 0, z = 0 },
		look_position = { x = 10, y = 0, z = 0 },
		start_calls = 0,
		start = function(self) self.start_calls = self.start_calls + 1 end,
		stop = function() end,
	}
	return {
		object = npc,
		st = {
			cover_name = cover,
			animpoint = point,
			reach_movement = {},
			reach_distance = 50,
		},
	}
end

assert(loadfile(script_path))()
on_game_start()
local wrapped_initialize = xr_animpoint.action_animpoint.initialize
local wrapped_reach = xr_animpoint.action_reach_animpoint.execute
local wrapped_motion = state_mgr_animation.animation.add_anim
on_game_start()
assert(xr_animpoint.action_animpoint.initialize == wrapped_initialize, "initialize wrapped twice")
assert(xr_animpoint.action_reach_animpoint.execute == wrapped_reach, "reach execute wrapped twice")
assert(state_mgr_animation.animation.add_anim == wrapped_motion, "add_anim wrapped twice")

local pri_npc = make_npc(1, "pri_a15_actor")
local pri_action = make_action(pri_npc, "pri_a15_animpoint_actor")
wrapped_initialize(pri_action)
assert(base_initialize_calls == 1, "retail initialize did not call action_base")
assert(pri_action.st.animpoint.start_calls == 1, "retail initialize did not start animpoint")
assert(original_initialize_calls == 0, "scoped initialize delegated to Anomaly idle body")
assert(#state_calls == 0, "scoped initialize forced an idle state")

wrapped_reach(pri_action)
assert(base_execute_calls == 1, "retail reach did not call action_base")
assert(pri_npc.dest_vertex == 123, "retail reach did not set destination vertex")
assert(pri_npc.desired_direction == pri_action.st.animpoint.smart_direction,
	"retail reach did not restore authored smart-cover heading")
assert(pri_npc.path_type == game_object.level_path, "retail reach did not set level path")
assert(#state_calls == 1 and state_calls[1].state == "walk", "retail reach state changed")
assert(original_reach_calls == 0, "scoped reach delegated to Anomaly body")

local finalized = xr_animpoint.action_animpoint.finalize(pri_action)
assert(finalized == "finalized" and original_finalize_calls == 1,
	"diagnostic finalize did not preserve the original result")

local animation = {
	npc = pri_npc,
	mgr = { target_state = "pri_a15_actor_all" },
	states = { anim_marker = "in" },
}
local motion_result = wrapped_motion(animation, "pri_a15_igrok_cam13", { prop = { moving = true } })
assert(motion_result == "motion:pri_a15_igrok_cam13", "motion result changed")
assert(original_motion_calls == 1 and pri_npc.animations == 1, "motion queue call changed")

current_level = "jupiter"
local jup_npc = make_npc(2, "jup_b219_stalker_tech")
local jup_action = make_action(jup_npc, "jup_b219_azot_smart_cover")
wrapped_initialize(jup_action)
assert(base_initialize_calls == 2 and jup_action.st.animpoint.start_calls == 1,
	"JUP retail initialize was not applied")

local delegate_cases = {
	{ level = "jupiter_underground", section = "pas_b400_zulus", cover = "pas_b400_cover" },
	{ level = "zaton", section = "pri_a15_actor", cover = "pri_a15_animpoint_actor" },
	{ level = "pripyat", section = "sim_default_stalker_1", cover = "pri_a15_animpoint_actor" },
	{ level = "pripyat", section = "pri_a15_actor", cover = "wrong_cover" },
}
for index, case in ipairs(delegate_cases) do
	current_level = case.level
	local npc = make_npc(100 + index, case.section)
	local action = make_action(npc, case.cover)
	wrapped_initialize(action)
end
assert(original_initialize_calls == #delegate_cases, "out-of-scope initialize was intercepted")

local saw_active = false
local saw_initialize = false
local saw_heading = false
local saw_motion = false
for _, line in ipairs(log_lines) do
	if string.find(line, "retail CoP action lifecycle active", 1, true) then saw_active = true end
	if string.find(line, "retail initialize npc=pri_a15_actor_object", 1, true) then saw_initialize = true end
	if string.find(line, "retail reach heading npc=pri_a15_actor_object", 1, true) then saw_heading = true end
	if string.find(line, "motion=pri_a15_igrok_cam13 count=1", 1, true) then saw_motion = true end
end
assert(saw_active, "v120 activation was not logged")
assert(saw_initialize, "v120 initialize was not logged")
assert(saw_heading, "v120 reach heading was not logged")
assert(saw_motion, "v120 motion enqueue was not logged")

print("v120 retail CoP action lifecycle smoke test passed")
