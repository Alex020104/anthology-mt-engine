local patch_path = assert(arg[1], "path to v107 script is required")

local function vec(x, y, z)
	return {
		x = x or 0,
		y = y or 0,
		z = z or 0,
		set = function(self, a, b, c)
			if type(a) == "table" then
				self.x, self.y, self.z = a.x, a.y, a.z
			else
				self.x, self.y, self.z = a, b, c
			end
			return self
		end,
		distance_to_sqr = function(self, other)
			local dx, dy, dz = self.x - other.x, self.y - other.y, self.z - other.z
			return dx * dx + dy * dy + dz * dz
		end,
	}
end

vector = function()
	return vec()
end

local paths = {
	guard_path = { position = vec(301, 0, 0), vertex = 301 },
	camper_path = { position = vec(401, 0, 0), vertex = 401 },
}

patrol = function(name)
	local data = assert(paths[name], "unknown test patrol " .. tostring(name))
	return {
		count = function() return 1 end,
		point = function() return data.position end,
		level_vertex_id = function() return data.vertex end,
	}
end

level = {
	vertex_position = function(level_vertex)
		return vec(level_vertex * 10, 0, 0)
	end,
	vertex_id = function(position)
		return math.floor(position.x)
	end,
	patrol_path_exists = function(name)
		return paths[name] ~= nil
	end,
}

db = {
	storage = {},
	offline_objects = {},
	spawned_vertex_by_id = {},
}

printf = function()
end

local smartcovers = {
	base_cover = { position = vec(501, 0, 0) },
	zero_cover = { position = vec(0, 0, 0) },
}
se_smart_cover = { registered_smartcovers = smartcovers }

local function ini_for(point)
	return {
		r_string_ex = function(_, _, key)
			if key == "pt1" then
				return "pos:" .. tostring(point) .. ",0,0 animpoint:" .. tostring(point) .. ",0,0"
			end
		end,
	}
end

xr_logic_ex = {}
xr_logic_ex.load_animpoint = function(npc)
	local root = db.storage[npc:id()]
	local position = vec(root.test_point, 0, 0)
	local desired = {
		active_section = root.active_section,
		custom_logic = "animpoint",
		animpoint = position,
		position = position,
		level_vertex_id = root.test_point,
	}
	root.beh.desired_target = desired
	return desired
end

smart_terrain = { se_smart_terrain = {} }
local setup_positions = {}
smart_terrain.se_smart_terrain.setup_logic = function(_, npc)
	local id = npc:id()
	if id == 1 or id == 2 or id == 6 then
		local point = id == 1 and 101 or (id == 2 and 202 or 606)
		db.storage[id] = {
			active_scheme = "beh",
			active_section = "camp_work_" .. tostring(id),
			ini = ini_for(point),
			beh = {},
			test_point = point,
		}
	elseif id == 3 then
		db.storage[id] = {
			active_scheme = "walker",
			active_section = "guard_work",
			walker = { path_walk = "guard_path" },
		}
	elseif id == 4 then
		db.storage[id] = {
			active_scheme = "camper",
			active_section = "camper_work",
			camper = { path_walk = "camper_path" },
		}
	elseif id == 5 then
		db.storage[id] = {
			active_scheme = "animpoint",
			active_section = "cover_work",
			animpoint = { cover_name = "base_cover" },
		}
	elseif id == 7 then
		-- Stock animpoint storage may contain a zero-vector placeholder while
		-- its smart-cover is absent or not registered yet. It is not a target.
		db.storage[id] = {
			active_scheme = "animpoint",
			active_section = "missing_cover_work",
			animpoint = {
				cover_name = "missing_cover",
				position = vec(0, 0, 0),
				level_vertex_id = 777,
			},
		}
	elseif id == 8 then
		db.storage[id] = {
			active_scheme = "animpoint",
			active_section = "zero_cover_work",
			animpoint = { cover_name = "zero_cover" },
		}
	end
	setup_positions[id] = npc:position().x
end

local function make_smart(name, ids, arriving)
	local infos = {}
	for _, id in ipairs(ids) do
		infos[id] = { job = { section = "logic@" .. name .. "_work_" .. tostring(id) } }
	end
	local smart = {
		position = vec(0, 0, 0),
		npc_info = infos,
		arriving_npc = arriving or {},
	}
	function smart:name()
		return name
	end
	return setmetatable(smart, { __index = smart_terrain.se_smart_terrain })
end

local base = make_smart("base", { 1, 2, 3, 4, 5, 7, 8 })
local arrival = make_smart("arrival", { 6 }, { [6] = true })
local smarts = { [51] = base, [52] = arrival }

alife = function()
	return {
		object = function(_, smart_id)
			return smarts[smart_id]
		end,
	}
end

smart_terrain.setup_gulag_and_logic_on_spawn = function(npc, _, se_obj)
	local smart = smarts[se_obj.m_smart_terrain_id]
	if smart and smart.npc_info[se_obj.id] then
		smart:setup_logic(npc)
	end
end

local function npc(id)
	local object = { object_id = id, current_position = vec(-1, 0, 0) }
	function object:id()
		return self.object_id
	end
	function object:name()
		return "npc_" .. tostring(self.object_id)
	end
	function object:position()
		return self.current_position
	end
	function object:set_npc_position(position)
		self.current_position = vec(position.x, position.y, position.z)
	end
	return object
end

xr_motivator = { motivator_binder = {} }
xr_motivator.motivator_binder.net_spawn = function(self, se_obj)
	smart_terrain.setup_gulag_and_logic_on_spawn(self.object, nil, se_obj, 0, true)
	local redirect = db.spawned_vertex_by_id[se_obj.id]
	if redirect then
		-- Model the effective Exo binder overwriting exact coordinates with a
		-- coarse navigation-node position after smart setup.
		self.object:set_npc_position(level.vertex_position(redirect))
		db.spawned_vertex_by_id[se_obj.id] = nil
	end
	return true
end

dofile(patch_path)
on_game_start()

local function spawn(id, smart_id, server_x)
	local object = npc(id)
	local se_obj = { id = id, m_smart_terrain_id = smart_id, position = vec(server_x, 0, 0) }
	xr_motivator.motivator_binder.net_spawn({ object = object }, se_obj)
	return object
end

local camp_a = spawn(1, 51, 0)
local camp_b = spawn(2, 51, 0)
local walker = spawn(3, 51, 0)
local camper = spawn(4, 51, 0)
local animpoint = spawn(5, 51, 0)
local far_arrival = spawn(6, 52, 100)
local missing_cover = spawn(7, 51, 0)
local zero_cover = spawn(8, 51, 0)

assert(camp_a:position().x == 101, "first camp worker stayed at the common smart centre")
assert(camp_b:position().x == 202, "second camp worker did not receive its distinct pt1")
assert(camp_a:position().x ~= camp_b:position().x, "camp workers still share one work position")
assert(walker:position().x == 301, "walker did not start at the assigned work path")
assert(camper:position().x == 401, "camper did not start at the assigned work path")
assert(animpoint:position().x == 501, "animpoint NPC did not start at its smart-cover")
assert(far_arrival:position().x == -1, "real far arrival was teleported")
assert(missing_cover:position().x == -1, "missing smart-cover teleported NPC to a zero placeholder")
assert(db.spawned_vertex_by_id[7] == nil, "missing smart-cover seeded an unsafe redirect")
assert(zero_cover:position().x == -1, "zero smart-cover placeholder moved an NPC")
assert(db.spawned_vertex_by_id[8] == nil, "zero smart-cover placeholder seeded a redirect")
assert(db.storage[1].beh.desired_target, "beh target was not initialized before the first update")
assert(db.storage[2].beh.desired_target, "second beh target was not initialized before the first update")

print("v107 active scheme placement smoke test: PASS")
