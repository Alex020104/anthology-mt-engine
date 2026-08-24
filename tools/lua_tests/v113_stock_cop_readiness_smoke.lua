local gamedata = assert(arg[1], "path to v113 gamedata is required")
local scripts = gamedata .. "\\scripts\\"
local configs = gamedata .. "\\configs\\"

local function read_all(path)
	local file = assert(io.open(path, "rb"))
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
function CreateTimeEvent() end
function RemoveTimeEvent() end
function printf() end

local info_set = { cop_start = true }
function has_alife_info(name)
	return info_set[name] == true
end

local current_level = "pripyat"
level = {
	name = function() return current_level end,
	patrol_path_exists = function() return true end,
}

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

local stories = {}
local storage = {}
local covers_by_vertex = {}
local function make_position(x)
	return {
		x = x, y = 0, z = 0,
		distance_to_sqr = function(self, other)
			local dx, dy, dz = self.x - other.x, self.y - other.y, self.z - other.z
			return dx * dx + dy * dy + dz * dz
		end,
	}
end
local function make_object(id, position)
	return {
		id = function() return id end,
		position = function() return position end,
	}
end

se_smart_cover = { registered_smartcovers = {} }
for index, entry in ipairs(pri_cast) do
	local cover = {
		m_game_vertex_id = 1000 + index,
		m_level_vertex_id = 2000 + index,
		position = make_position(index),
	}
	se_smart_cover.registered_smartcovers[entry[2]] = cover
	covers_by_vertex[cover.m_game_vertex_id] = cover
	local object = make_object(index, cover.position)
	stories[entry[1]] = object
	storage[index] = {
		active_scheme = "animpoint",
		animpoint = {
			cover_name = entry[2],
			animpoint = { started = true, cover_name = entry[2] },
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

local active_by_logic = {}
local cover_by_active = {}
local jobs = {}
for index, entry in ipairs(pri_cast) do
	local logic = "logic@test_" .. index
	local active = "animpoint@test_" .. index
	active_by_logic[logic] = active
	cover_by_active[active] = entry[2]
	jobs[index] = { section = logic }
end
local ltx = {}
function ltx:line_exist(section, key)
	return key == "active" and active_by_logic[section] ~= nil
end
function ltx:r_string_ex(section, key)
	if key == "active" then return active_by_logic[section] end
	if key == "cover_name" then return cover_by_active[section] end
end

local smart = {
	is_on_actor_level = true,
	disabled = false,
	ltx = ltx,
	stalker_jobs = jobs,
	monster_jobs = {},
	heli_jobs = {},
	name = function() return "pri_a15" end,
}
-- The regression under test: values are wrappers, not smart terrains.
SIMBOARD = { smarts = { [77] = { smrt = smart } } }
gulag_general = { get_job_type = function() return "smartcover_job" end }
function CALifeSmartTerrainTask(game_vertex_id)
	local cover = assert(covers_by_vertex[game_vertex_id])
	return {
		game_vertex_id = function() return cover.m_game_vertex_id end,
		position = function() return cover.position end,
	}
end
function game_graph()
	return {
		vertex = function()
			return { level_id = function() return 30 end }
		end,
	}
end

local readiness_path = scripts .. "zzzzzzzzzzz_anthology_cutscenes_v113_readiness.script"
local readiness_source = read_all(readiness_path)
assert(not readiness_source:find("set_npc_position", 1, true), "direct NPC movement returned")
assert(not readiness_source:find("server.position", 1, true), "server position staging returned")
assert(loadfile(readiness_path))()
assert(type(anthology_cutscenes_v113_after_id_cleaner_spawn) == "function", "ID Cleaner hook missing")
on_game_start()
anthology_cutscenes_v113_after_id_cleaner_spawn(30)

for index, job in ipairs(jobs) do
	assert(job.alife_task, "wrapper smart job was not rebound")
	assert(job.position == se_smart_cover.registered_smartcovers[pri_cast[index][2]].position)
end
for _ = 1, 3 do
	for _, callback in ipairs(callback_lists.actor_on_update or {}) do callback() end
end
assert(info_set.anthology_v113_pri_a15_ready, "Pripyat animpoint readiness not published")

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
local patrol_points = {}
for index, entry in ipairs(pas_cast) do
	local point = make_position(100 + index)
	patrol_points[entry[3]] = point
	local id = 100 + index
	stories[entry[1]] = make_object(id, point)
	storage[id] = { active_scheme = "walker", active_section = entry[2] }
end
function patrol(name)
	local point = assert(patrol_points[name])
	return { point = function() return point end }
end
for _ = 1, 3 do
	for _, callback in ipairs(callback_lists.actor_on_update or {}) do callback() end
end
assert(info_set.anthology_v113_pas_b400_ready, "Underpass walker readiness not published")

local cleaner = read_all(scripts .. "anthology_id_cleaner.script")
assert(cleaner:find("anthology_cutscenes_v113_after_id_cleaner_spawn", 1, true), "post-spawn hook call missing")
local pri_scene = read_all(configs .. "scripts\\pripyat\\pri_a15_sr_cutscene.ltx")
assert(pri_scene:find("on_timer = 5000 | sr_idle@launch", 1, true), "Pripyat hard fallback missing")
local pas_scene = read_all(configs .. "scripts\\underpass\\pas_b400_sr_control.ltx")
assert(pas_scene:find("on_timer = 3000 | sr_idle@out", 1, true), "Underpass hard fallback missing")
local pri_logic = read_all(configs .. "scripts\\pripyat\\pri_a15_logic.ltx")
local sokolov = assert(pri_logic:find("[animpoint@sokolov]", 1, true))
assert(pri_logic:find("out_restr = pri_a15_sr_start", sokolov, true), "Sokolov out restrictor missing")
local squad_patch = read_all(configs .. "mod_system_anthology_cutscenes_v113.ltx")
assert(squad_patch:find("spawn_point = pas_b400_elevator_zulus_1_walk", 1, true), "Zulus spawn point missing")
local duplicate_sound = io.open(configs .. "scripts\\jupiter\\jup_b219_sr_sound_underpass.ltx", "rb")
assert(not duplicate_sound, "v113 regressed the v110 duplicate door-sound fix")

print("PASS v113 stock CoP readiness smoke")
