local gamedata = assert(arg[1], "path to v115 gamedata is required")
local scripts = gamedata .. "\\scripts\\"
local configs = gamedata .. "\\configs\\"

local function read_all(path)
	local file = assert(io.open(path, "rb"), path)
	local data = file:read("*a")
	file:close()
	return data
end

local callback_lists = {}
function RegisterScriptCallback(name, callback)
	callback_lists[name] = callback_lists[name] or {}
	table.insert(callback_lists[name], callback)
end
function UnregisterScriptCallback() end
function printf() end

local info_set = { cop_start = true }
function has_alife_info(name)
	return info_set[name] == true
end

local clock_ms = 0
function time_global()
	return clock_ms
end

local current_level = "pripyat"
local patrol_points = {}
level = {
	name = function() return current_level end,
	patrol_path_exists = function(name) return patrol_points[name] ~= nil end,
}
function patrol(name)
	return {
		point = function(_, index)
			assert(index == 0)
			return patrol_points[name]
		end,
	}
end

local function position(seed)
	return { x = seed, y = seed + 0.25, z = seed + 0.5 }
end

local stories = {}
local storage = {}
local live_positions = {}
local live_directions = {}
local function make_object(id)
	return {
		id = function() return id end,
		animation_count = function() return 1 end,
		position = function()
			local value = live_positions[id]
			return { x = value.x, y = value.y, z = value.z }
		end,
		direction = function()
			local value = live_directions[id]
			return { x = value.x, y = value.y, z = value.z }
		end,
	}
end

local pri_cast = {
	{ "pri_a15_actor",            "pri_a15_animpoint_actor" },
	{ "pri_a15_military_tarasov", "pri_a15_animpoint_military_1" },
	{ "pri_a15_military_2",       "pri_a15_animpoint_military_2" },
	{ "pri_a15_military_3",       "pri_a15_animpoint_military_3" },
	{ "pri_a15_military_4",       "pri_a15_animpoint_military_4" },
	{ "pri_a15_vano",             "pri_a15_animpoint_vano" },
	{ "pri_a15_sokolov_scene",    "pri_a15_animpoint_sokolov" },
	{ "pri_a15_zulus",            "pri_a15_animpoint_zulus" },
	{ "pri_a15_wanderer",         "pri_a15_animpoint_wanderer" },
}
for index, entry in ipairs(pri_cast) do
	local id = index
	local target = position(index)
	live_positions[id] = { x = target.x + 5, y = target.y, z = target.z }
	live_directions[id] = { x = 0, y = 0, z = 1 }
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "animpoint",
		animpoint = {
			cover_name = entry[2],
			animpoint = {
				started = true,
				current_action = "scene_idle",
				cover_name = entry[2],
				position = target,
				smart_direction = { x = 0, y = 0, z = 1 },
			},
		},
		state_mgr = {
			target_state = "scene_idle",
			animation_direction_applied = true,
			animation_position = target,
		},
	}
end

db = {
	actor = {
		give_info_portion = function(_, name)
			info_set[name] = true
		end,
	},
	storage = storage,
}
function get_story_object(name)
	return stories[name]
end

local staging_path = scripts .. "zzzzzzzzzzzz_anthology_cutscenes_v114_staging.script"
local staging_source = read_all(staging_path)
assert(not staging_source:find("set_npc_position", 1, true), "direct client NPC movement returned")
assert(not staging_source:find("server.position", 1, true), "direct server NPC movement returned")
assert(not staging_source:find("set_actor_position", 1, true), "actor placement leaked into NPC gate")
assert(loadfile(staging_path))()
on_game_start()

local function update(step_ms)
	clock_ms = clock_ms + (step_ms or 50)
	for _, callback in ipairs(callback_lists.actor_on_update or {}) do
		callback()
	end
end

for _ = 1, 12 do update() end
assert(not info_set.anthology_v114_pri_a15_ready, "Pripyat gate trusted target state instead of live XFORM")

for index = 1, #pri_cast do
	local target = storage[index].animpoint.animpoint.position
	live_positions[index] = { x = target.x, y = target.y, z = target.z }
end

-- Slow cumulative movement below the old per-frame 0.02 m tolerance must not
-- satisfy a 300 ms settle window.
local moving_target = storage[1].animpoint.animpoint.position
live_positions[1] = { x = moving_target.x - 0.10, y = moving_target.y, z = moving_target.z }
for _ = 1, 20 do
	live_positions[1].x = live_positions[1].x + 0.01
	update()
end
assert(not info_set.anthology_v114_pri_a15_ready, "Pripyat gate accepted cumulative slow movement")

for index = 1, #pri_cast do
	local target = storage[index].animpoint.animpoint.position
	live_positions[index] = { x = target.x, y = target.y, z = target.z }
end
live_directions[2] = { x = 1, y = 0, z = 0 }
for _ = 1, 10 do update() end
assert(not info_set.anthology_v114_pri_a15_ready, "Pripyat gate ignored live direction mismatch")
live_directions[2] = { x = 0, y = 0, z = 1 }
for _ = 1, 8 do update() end
assert(not info_set.anthology_v114_pri_a15_ready, "Pripyat gate skipped the timed settle window")
update()
assert(info_set.anthology_v114_pri_a15_ready, "Pripyat settled XFORM readiness was not published")

current_level = "none"
update()
info_set.anthology_v114_pas_b400_ready = nil
info_set.anthology_v114_pas_b400_fallback = nil
current_level = "jupiter_underground"
info_set.jup_a10_vano_agree_go_und = true
info_set.jup_b218_soldier_hired = true
info_set.jup_b218_monolith_hired = true

local pas_cast = {
	{ "pas_b400_zulus",    "walker@zulus_elevator_1",    "pas_b400_elevator_zulus_1_walk" },
	{ "pas_b400_vano",     "walker@vano_elevator_1",     "pas_b400_elevator_vano_1_walk" },
	{ "pas_b400_sokolov",  "walker@sokolov_elevator_1",  "pas_b400_elevator_sokolov_1_walk" },
	{ "pas_b400_wanderer", "walker@wanderer_elevator_1", "pas_b400_elevator_wanderer_1_walk" },
}
for index, entry in ipairs(pas_cast) do
	local id = 100 + index
	local target = position(id)
	patrol_points[entry[3]] = target
	live_positions[id] = { x = target.x + 4, y = target.y, z = target.z }
	live_directions[id] = { x = 0, y = 0, z = 1 }
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "walker",
		active_section = entry[2],
		move_mgr = {
			path_walk = entry[3],
			last_index = nil,
			arrived_to_first_waypoint = function() return false end,
		},
	}
end
for _ = 1, 6 do update() end
assert(not info_set.anthology_v114_pas_b400_ready, "Underpass gate ignored live waypoint distance")

for index = 1, #pas_cast do
	local id = 100 + index
	local target = patrol_points[pas_cast[index][3]]
	live_positions[id] = { x = target.x, y = target.y, z = target.z }
end
for _ = 1, 8 do update() end
assert(not info_set.anthology_v114_pas_b400_ready, "Underpass gate skipped the timed settle window")
update()
assert(info_set.anthology_v114_pas_b400_ready, "Underpass physical waypoint readiness was not published")

current_level = "none"
update()
info_set.anthology_v114_pas_b400_ready = nil
info_set.anthology_v114_pas_b400_fallback = nil
current_level = "jupiter_underground"
live_positions[101].x = live_positions[101].x + 10
clock_ms = 100
update(0)
clock_ms = 20099
update(0)
assert(not info_set.anthology_v114_pas_b400_fallback, "Underpass fallback fired early")
clock_ms = 20100
update(0)
assert(info_set.anthology_v114_pas_b400_fallback, "Underpass diagnosed fallback did not fire")

local pas_scene = read_all(configs .. "scripts\\underpass\\pas_b400_sr_control.ltx")
assert(pas_scene:find("on_game_timer = 300 | sr_idle@out", 1, true), "Underpass hard fallback still counts level I/O")
assert(not pas_scene:find("on_timer = 20000 | sr_idle@out", 1, true), "v114 real-time Underpass fallback returned")

print("PASS v115 settled CoP NPC staging smoke")
