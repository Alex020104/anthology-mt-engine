local gamedata = assert(arg[1], "path to v117 gamedata is required")
local scripts = gamedata .. "\\scripts\\"
local configs = gamedata .. "\\configs\\"

local function read_all(path)
	local file = assert(io.open(path, "rb"), path)
	local value = file:read("*a")
	file:close()
	return value
end

local callbacks = {}
function RegisterScriptCallback(name, callback)
	callbacks[name] = callbacks[name] or {}
	table.insert(callbacks[name], callback)
end
function UnregisterScriptCallback() end
function printf() end

local info_set = {
	cop_start = true,
	jup_b218_gather_squad_complete = true,
	jup_a10_vano_agree_go_und = true,
	jup_b218_soldier_hired = true,
	jup_b218_monolith_hired = true,
	jup_b219_actor_on_pos = true,
	jup_b219_azot_on_pos = true,
	jup_b219_zulus_on_pos = true,
	jup_b219_vano_on_pos = true,
	jup_b219_soldier_on_pos = true,
	jup_b219_monolith_on_pos = true,
}
function has_alife_info(name)
	return info_set[name] == true
end

local clock_ms = 0
function time_global()
	return clock_ms
end

local current_level = "jupiter"
local patrol_points = {}
level = {
	name = function()
		return current_level
	end,
	-- This API is unreliable in the live Anomaly runtime. The authored patrol
	-- object is still valid and must be queried directly.
	patrol_path_exists = function()
		return false
	end,
}
function patrol(name)
	return {
		point = function(_, index)
			assert(index == 0)
			return patrol_points[name]
		end,
	}
end

local stories = {}
local storage = {}
local positions = {}
local directions = {}
local function make_object(id)
	return {
		id = function()
			return id
		end,
		animation_count = function()
			return 1
		end,
		position = function()
			local p = positions[id]
			return { x = p.x, y = p.y, z = p.z }
		end,
		direction = function()
			local d = directions[id]
			return { x = d.x, y = d.y, z = d.z }
		end,
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

local function target(seed)
	return { x = seed, y = 0, z = seed * 2 }
end

local animpoints = {
	{ "jup_b219_actor", "jup_b219_actor_smart_cover" },
	{ "jup_b219_stalker_tech_id", "jup_b219_azot_smart_cover" },
}
for index, entry in ipairs(animpoints) do
	local id = index
	local point = target(index)
	positions[id] = { x = point.x, y = point.y, z = point.z }
	directions[id] = { x = 0, y = 0, z = 1 }
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "animpoint",
		animpoint = {
			cover_name = entry[2],
			animpoint = {
				started = true,
				current_action = "pri_a15_idle_none",
				cover_name = entry[2],
				position = point,
				smart_direction = { x = 0, y = 0, z = 1 },
			},
		},
		state_mgr = {
			target_state = "pri_a15_idle_none",
			animation_direction_applied = true,
			animation_position = point,
		},
	}
end

local walkers = {
	{ "jup_b219_zulus_id", "jup_b219_zulus_walk", "jup_b219_zulus_look" },
	{ "jup_b219_vano_id", "jup_b219_vano_walk", "jup_b219_vano_look" },
	{ "jup_b219_soldier_id", "jup_b219_soldier_walk", "jup_b219_soldier_look" },
	{ "jup_b219_monolith_squad_leader_freedom_skin_id", "jup_b219_monolith_walk", "jup_b219_monolith_look" },
}
for index, entry in ipairs(walkers) do
	local id = 10 + index
	local point = target(id)
	patrol_points[entry[2]] = point
	patrol_points[entry[3]] = { x = point.x, y = point.y, z = point.z + 5 }
	positions[id] = { x = point.x, y = point.y, z = point.z }
	directions[id] = { x = 0, y = 0, z = 1 }
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "walker",
		active_section = "walker",
		move_mgr = { path_walk = entry[2] },
	}
end

local staging_path = scripts .. "zzzzzzzzzzzz_anthology_cutscenes_v114_staging.script"
local source = read_all(staging_path)
assert(not source:find("set_npc_position", 1, true), "direct NPC movement returned")
assert(loadfile(staging_path))()
on_game_start()

local function update(step_ms)
	clock_ms = clock_ms + (step_ms or 50)
	for _, callback in ipairs(callbacks.actor_on_update or {}) do
		callback()
	end
end

-- The stock on_pos signal alone is insufficient: the body must finish turning.
directions[11] = { x = 1, y = 0, z = 0 }
for _ = 1, 20 do
	update()
end
assert(not info_set.anthology_v116_jup_b219_ready, "Jupiter gate ignored the authored look direction")

directions[11] = { x = 0, y = 0, z = 1 }
positions[11].x = positions[11].x + 0.25
for _ = 1, 20 do
	update()
end
assert(not info_set.anthology_v116_jup_b219_ready, "Jupiter gate accepted a visible 25 cm XZ offset")
positions[11].x = positions[11].x - 0.25
for _ = 1, 8 do
	update()
end
assert(not info_set.anthology_v116_jup_b219_ready, "Jupiter gate skipped the timed settle window")
update()
assert(info_set.anthology_v116_jup_b219_ready, "Jupiter settled position/direction was not published")

-- A missing stock on_pos marker must not consume the fallback during spawning.
current_level = "none"
update()
info_set.anthology_v116_jup_b219_ready = nil
info_set.anthology_v116_jup_b219_fallback = nil
info_set.jup_b219_zulus_on_pos = nil
current_level = "jupiter"
clock_ms = 30000
update(0)
assert(not info_set.anthology_v116_jup_b219_fallback, "Jupiter fallback started before stock arrival")
info_set.jup_b219_zulus_on_pos = true
directions[11] = { x = 1, y = 0, z = 0 }
update(0)
clock_ms = 59999
update(0)
assert(not info_set.anthology_v116_jup_b219_fallback, "Jupiter fallback fired early")
clock_ms = 60000
update(0)
assert(info_set.anthology_v116_jup_b219_fallback, "Jupiter diagnosed fallback did not fire")
assert(info_set.anthology_v116_jup_b219_ready, "Jupiter fallback did not preserve quest progress")

local control = read_all(configs .. "scripts\\jupiter\\jup_b219_sr_control.ltx")
local ready_count = 0
for _ in control:gmatch("%+anthology_v116_jup_b219_ready") do
	ready_count = ready_count + 1
end
assert(ready_count == 8, "not all eight Jupiter variants wait for v116 readiness")
local hard_fallback_count = 0
for _ in control:gmatch("on_timer = 30000") do
	 hard_fallback_count = hard_fallback_count + 1
end
assert(hard_fallback_count == 8, "not all eight Jupiter variants have a hard fallback")

print("PASS v117 Jupiter grounded settle smoke")
