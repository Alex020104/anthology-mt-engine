local staging_path = assert(arg[1], "staging script path is required")
local id_cleaner_path = assert(arg[2], "id cleaner path is required")
local underpass_path = assert(arg[3], "Underpass config path is required")
local pripyat_path = assert(arg[4], "Pripyat config path is required")
local sar_path = assert(arg[5], "SAR script path is required")

local function read_all(path)
	local file = assert(io.open(path, "rb"))
	local data = file:read("*a")
	file:close()
	return data
end

local logs = {}
function printf(_, message)
	logs[#logs + 1] = tostring(message or _)
end

function vector()
	local result = { x = 0, y = 0, z = 0 }
	function result:set(value)
		self.x = value.x
		self.y = value.y
		self.z = value.z
		return self
	end
	return result
end

local function pos(seed)
	return { x = seed, y = seed + 0.25, z = seed + 0.5 }
end

local critical_covers = {
	"pri_a15_animpoint_actor",
	"pri_a15_animpoint_vano",
	"pri_a15_animpoint_sokolov",
	"pri_a15_animpoint_zulus",
	"pri_a15_animpoint_wanderer",
	"pri_a15_animpoint_military_1",
	"pri_a15_animpoint_military_2",
	"pri_a15_animpoint_military_3",
	"pri_a15_animpoint_military_4",
}

local cover_by_vertex = {}
se_smart_cover = { registered_smartcovers = {} }
for index, name in ipairs(critical_covers) do
	local cover = {
		position = pos(index * 10),
		m_level_vertex_id = index * 100,
		m_game_vertex_id = index * 1000,
	}
	se_smart_cover.registered_smartcovers[name] = cover
	cover_by_vertex[cover.m_game_vertex_id] = cover
end
se_smart_cover.registered_smartcovers.pri_a16_animp_vano = {
	position = pos(500),
	m_level_vertex_id = 5000,
	m_game_vertex_id = 50000,
}

local story_ids = {}
local clients = {}
local function add_story(name, object_id)
	story_ids[name] = object_id
	clients[object_id] = {
		id_value = object_id,
		set_npc_position = function(self, value)
			self.last_position = value
		end,
	}
end

local pri_stories = {
	"pri_a15_actor", "pri_a15_vano", "pri_a15_sokolov_scene",
	"pri_a15_zulus", "pri_a15_wanderer", "pri_a15_military_tarasov",
	"pri_a15_military_2", "pri_a15_military_3", "pri_a15_military_4",
}
for index, story in ipairs(pri_stories) do
	add_story(story, 100 + index)
end
local pas_stories = {
	"pas_b400_zulus", "pas_b400_vano", "pas_b400_sokolov", "pas_b400_wanderer",
}
for index, story in ipairs(pas_stories) do
	add_story(story, 200 + index)
end
add_story("pri_a16_vano", 501)

function get_story_object_id(story_id)
	return story_ids[story_id]
end

function get_story_object(story_id)
	return clients[story_ids[story_id]]
end

function alife_object(object_id)
	return clients[object_id]
end

level = {
	name = function()
		return "pripyat"
	end,
	object_by_id = function(object_id)
		return clients[object_id]
	end,
	patrol_path_exists = function()
		return true
	end,
}

function patrol(path_name)
	local seed = 700 + #path_name
	return {
		point = function()
			return pos(seed)
		end,
		level_vertex_id = function()
			return seed * 10
		end,
		game_vertex_id = function()
			return seed * 100
		end,
	}
end

local ready_info = false
db = {
	actor = {
		give_info_portion = function(_, name)
			if name == "anthology_v112_pri_stage_ready" then
				ready_info = true
			end
		end,
	},
	spawned_vertex_by_id = {},
	storage = {
		[501] = {
			active_scheme = "animpoint",
			animpoint = { cover_name = "pri_a16_animp_vano" },
		},
	},
}

local job_ltx = {}
function job_ltx:line_exist(section, key)
	return section == "logic@test" and key == "active"
end
function job_ltx:r_string_ex(section, key)
	if section == "logic@test" and key == "active" then
		return "animpoint@test"
	end
	if section == "animpoint@test" and key == "cover_name" then
		return "pri_a15_animpoint_actor"
	end
	return nil
end

local test_job = { section = "logic@test", ltx = job_ltx }
local test_smart = {
	is_on_actor_level = true,
	disabled = false,
	stalker_jobs = { test_job },
	monster_jobs = {},
	heli_jobs = {},
	name = function()
		return "pri_a15"
	end,
}
SIMBOARD = { smarts = { test_smart } }
gulag_general = {
	get_job_type = function()
		return "smartcover_job"
	end,
}

function game_graph()
	return {
		vertex = function()
			return { level_id = function() return 30 end }
		end,
	}
end

function CALifeSmartTerrainTask(game_vertex_id)
	local cover = assert(cover_by_vertex[game_vertex_id])
	return {
		game_vertex_id = function()
			return cover.m_game_vertex_id
		end,
		position = function()
			return cover.position
		end,
	}
end

xr_effects = {}
function CreateTimeEvent() end
function RemoveTimeEvent() end

assert(loadfile(staging_path))()
assert(type(anthology_cutscenes_v112_after_id_cleaner_spawn) == "function")
assert(type(on_game_start) == "function")
on_game_start()
assert(type(xr_effects.anthology_v112_stage_pri_a15) == "function")
assert(type(xr_effects.anthology_v112_stage_pas_b400) == "function")

anthology_cutscenes_v112_after_id_cleaner_spawn(30)
assert(test_job.anthology_v112_cover_name == "pri_a15_animpoint_actor")
assert(test_job.position == se_smart_cover.registered_smartcovers.pri_a15_animpoint_actor.position)
assert(ready_info, "Pripyat scene readiness was not published")
assert(clients[501].last_position.x == 500, "active pri_a16 animpoint was not staged")

xr_effects.anthology_v112_stage_pri_a15()
for index, story in ipairs(pri_stories) do
	local object = clients[story_ids[story]]
	assert(object.last_position, story .. " was not moved")
	assert(object.last_position.x == index * 10, story .. " used the wrong cover")
end

xr_effects.anthology_v112_stage_pas_b400()
for _, story in ipairs(pas_stories) do
	assert(clients[story_ids[story]].last_position, story .. " was not moved to its path")
end

local id_cleaner = read_all(id_cleaner_path)
local actor_spawn = assert(id_cleaner:find("spawn_level%(current_level_id%(%)%)"))
local actor_notify = assert(id_cleaner:find("notify_after_level_spawn%(current_level_id%(%)%)", actor_spawn))
assert(actor_notify > actor_spawn, "ID Cleaner notifies before spawning the level")

local underpass = read_all(underpass_path)
assert(underpass:find("%[sr_idle@stage%]"))
assert(underpass:find("anthology_v112_stage_pas_b400"))

local pripyat = read_all(pripyat_path)
local stage_pos = assert(pripyat:find("anthology_v112_stage_pri_a15"))
local black_pos = assert(pripyat:find("=stop_postprocess%(3009%)", stage_pos))
assert(black_pos > stage_pos, "Pripyat black screen is removed before staging")

local sar = read_all(sar_path)
assert(sar:find("vector%(%)%:set%(device%(%)%.cam_pos%)"))
assert(sar:find("return getActorPos%(%)"))

print("PASS v112 deterministic CoP staging smoke")
