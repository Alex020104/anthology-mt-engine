local addon_root = assert(arg[1], "path to v111 gamedata is required")
local scripts = addon_root .. "\\scripts\\"

local cleaner_path = scripts .. "anthology_id_cleaner.script"
local cleaner = assert(io.open(cleaner_path, "rb")):read("*a")
assert(cleaner:find("smart_cover%s*=%s*true"), "smart_cover is not permanently protected")
assert(cleaner:find('entry%.section%s*~=%s*"smart_cover"'), "legacy smart-cover restore path is missing")
assert(cleaner:find('entry%.section%s*==%s*"smart_cover"'), "per-level legacy migration is missing")
assert(cleaner:find('return false, "pending"'), "missing legacy cover is discarded before its level loads")

local callbacks = {}
RegisterScriptCallback = function(name, callback)
	callbacks[name] = callback
end
UnregisterScriptCallback = function() end
CreateTimeEvent = function() end
RemoveTimeEvent = function() end
ResetTimeEvent = function() end
printf = function() end

local cover = { m_game_vertex_id = 12, m_level_vertex_id = 34 }
se_smart_cover = { registered_smartcovers = { pri_a15_animpoint_vano = cover } }

local ltx = {}
function ltx:line_exist(section, key)
	return section == "logic@vano" and key == "active"
end
function ltx:r_string_ex(section, key)
	if section == "logic@vano" and key == "active" then
		return "animpoint@vano"
	end
	if section == "animpoint@vano" and key == "cover_name" then
		return "pri_a15_animpoint_vano"
	end
end

local job = { section = "logic@vano" }
local smart = {
	is_on_actor_level = true,
	disabled = false,
	ltx = ltx,
	stalker_jobs = { job },
	monster_jobs = {},
	heli_jobs = {},
}
function smart:name() return "pri_a15" end
SIMBOARD = { smarts = { [1] = smart } }

gulag_general = {
	get_job_type = function(candidate)
		return candidate == job and "smartcover_job" or "path_job"
	end,
}

CALifeSmartTerrainTask = function(game_vertex, level_vertex)
	return {
		game_vertex_id = function() return game_vertex end,
		position = function() return { x = level_vertex, y = 0, z = 0 } end,
	}
end
game_graph = function()
	return {
		vertex = function(_, id)
			return { level_id = function() return id + 100 end }
		end,
	}
end

dofile(scripts .. "zzzzzzzzzzz_anthology_cutscenes_v111_smartcover_jobs.script")
on_game_start()
assert(type(callbacks.actor_on_first_update) == "function", "repair callback missing")
callbacks.actor_on_first_update()

assert(job.alife_task ~= nil, "smart-cover job task was not rebound")
assert(job.game_vertex_id == 12, "wrong rebound game vertex")
assert(job.level_id == 112, "wrong rebound level id")
assert(job.position.x == 34, "wrong rebound position")

local sar_calls = 0
sar_main = {
	actor_on_update = function()
		sar_calls = sar_calls + 1
	end,
}

local active_info = {}
has_alife_info = function(name)
	return active_info[name] == true
end

local actor_update_callbacks = {}
RegisterScriptCallback = function(name, callback)
	if name == "actor_on_update" then
		actor_update_callbacks[#actor_update_callbacks + 1] = callback
	else
		callbacks[name] = callback
	end
end
UnregisterScriptCallback = function(name, callback)
	if name == "actor_on_update" and callback == sar_main.actor_on_update then
		actor_update_callbacks = {}
	end
end

dofile(scripts .. "zzzzzzzzzzz_anthology_cutscenes_v111_sar_guard.script")
on_game_start()

local guarded_sar_update = actor_update_callbacks[#actor_update_callbacks]
assert(type(guarded_sar_update) == "function", "SAR cutscene guard callback missing")

guarded_sar_update()
assert(sar_calls == 1, "SAR update is blocked outside a cutscene")

active_info.jup_b219_start_sound = true
guarded_sar_update()
assert(sar_calls == 1, "SAR update is not frozen during the Jupiter descent")

active_info.jup_b219_destroy_actor = true
guarded_sar_update()
assert(sar_calls == 2, "SAR update is not restored after the Jupiter descent")

active_info.pri_a15_cutscene_go = true
guarded_sar_update()
assert(sar_calls == 2, "SAR update is not frozen during the Pripyat arrival")

active_info.pri_a15_cutscene_end = true
guarded_sar_update()
assert(sar_calls == 3, "SAR update is not restored after the Pripyat arrival")

print("v111 persistent smart-cover smoke test: PASS")
