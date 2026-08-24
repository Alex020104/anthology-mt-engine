local gamedata = assert(arg[1], "path to v114 gamedata is required")
local repo_root = assert(arg[2], "repository root is required")
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
level = { name = function() return current_level end }

local function position(seed)
	return { x = seed, y = seed + 0.25, z = seed + 0.5 }
end

local stories = {}
local storage = {}
local function make_object(id)
	return {
		id = function() return id end,
		animation_count = function() return 1 end,
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
	local root_position = position(index)
	local controller_position = position(index)
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "animpoint",
		animpoint = {
			cover_name = entry[2],
			animpoint = {
				started = true,
				current_action = "scene_idle",
				cover_name = entry[2],
				position = controller_position,
			},
		},
		state_mgr = {
			target_state = "scene_idle",
			animation_direction_applied = true,
			animation_position = root_position,
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
assert(loadfile(staging_path))()
on_game_start()

local function update()
	for _, callback in ipairs(callback_lists.actor_on_update or {}) do
		callback()
	end
end

for _ = 1, 4 do update() end
assert(not info_set.anthology_v114_pri_a15_ready, "Pripyat gate skipped stable updates")
update()
assert(info_set.anthology_v114_pri_a15_ready, "Pripyat authored staging was not published")

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
	stories[entry[1]] = make_object(id)
	storage[id] = {
		active_scheme = "walker",
		active_section = entry[2],
		move_mgr = {
			path_walk = entry[3],
			last_index = 0,
			arrived_to_first_waypoint = function() return true end,
		},
	}
end
for _ = 1, 4 do update() end
assert(not info_set.anthology_v114_pas_b400_ready, "Underpass gate skipped stable updates")
update()
assert(info_set.anthology_v114_pas_b400_ready, "Underpass first-waypoint readiness was not published")

-- The diagnosed fallback is measured from live actor updates, not level I/O.
current_level = "none"
info_set.anthology_v114_pas_b400_ready = nil
info_set.anthology_v114_pas_b400_fallback = nil
update()
current_level = "jupiter_underground"
storage[101].move_mgr.arrived_to_first_waypoint = function() return false end
clock_ms = 100
update()
clock_ms = 10099
update()
assert(not info_set.anthology_v114_pas_b400_fallback, "Underpass fallback fired early")
clock_ms = 10100
update()
assert(info_set.anthology_v114_pas_b400_fallback, "Underpass diagnosed fallback did not fire")

local squad_patch = read_all(configs .. "mod_system_anthology_cutscenes_v114.ltx")
assert(squad_patch:find("spawn_point = pas_b400_elevator_zulus_1_walk", 1, true))
assert(squad_patch:find("spawn_point = pri_a15_actor_spawn", 1, true))
assert(squad_patch:find("spawn_point = pri_a15_military_tarasov_spawn", 1, true))

local pri_scene = read_all(configs .. "scripts\\pripyat\\pri_a15_sr_cutscene.ltx")
assert(pri_scene:find("+anthology_v114_pri_a15_ready", 1, true))
assert(pri_scene:find("on_timer = 20000 | sr_idle@launch", 1, true))
local pas_scene = read_all(configs .. "scripts\\underpass\\pas_b400_sr_control.ltx")
assert(pas_scene:find("+anthology_v114_pas_b400_ready", 1, true))
assert(pas_scene:find("on_timer = 20000 | sr_idle@out", 1, true))

local camera_header = read_all(repo_root .. "\\src\\xrGame\\ActorEffector.h")
local camera_source = read_all(repo_root .. "\\src\\xrGame\\ActorEffector_script.cpp")
assert(camera_header:find("void Start(LPCSTR fn)", 1, true))
assert(camera_source:find("inherited::Start(fn)", 1, true))
assert(camera_source:find("m_objectAnimator->Update(Device.fTimeDelta)", 1, true))
assert(not camera_source:find("m_last_camera_info", 1, true), "stale camera-frame cache returned")

print("PASS v114 seamless CoP staging smoke")
