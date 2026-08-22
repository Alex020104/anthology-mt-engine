local patch_path = assert(arg[1], "path to v106 script is required")

local function vec(x)
	return {
		x = x,
		set = function(self, other)
			self.x = other.x
			return self
		end,
		distance_to_sqr = function(self, other)
			local delta = self.x - other.x
			return delta * delta
		end,
	}
end

vector = function()
	return vec(0)
end

level = {
	vertex_position = function(level_vertex)
		return vec(level_vertex * 10)
	end,
}

db = {
	offline_objects = {
		[1] = { level_vertex_id = 10 },
		[2] = { level_vertex_id = 20 },
		[3] = { level_vertex_id = 30 },
	},
	spawned_vertex_by_id = {},
}

printf = function()
end

local function task(level_vertex, exact_x)
	return {
		level_vertex_id = function()
			return level_vertex
		end,
		position = function()
			return vec(exact_x)
		end,
	}
end

local setup_positions = {}
smart_terrain = {
	se_smart_terrain = {},
}

smart_terrain.se_smart_terrain.setup_logic = function(_, npc)
	setup_positions[npc:id()] = npc:position().x
end

local function make_smart(name, position, infos, arriving)
	local smart = {
		position = vec(position),
		npc_info = infos,
		arriving_npc = arriving or {},
	}
	function smart:name()
		return name
	end
	function smart:select_npc_job(info)
		info.job = info.test_job
	end
	return setmetatable(smart, { __index = smart_terrain.se_smart_terrain })
end

local smart_a = make_smart("base", 0, {
	[1] = { job = { section = "guard", alife_task = task(101, 1001) } },
	[2] = { job = { section = "camp", alife_task = task(102, 1002) } },
})
local smart_near = make_smart("near", 0, {
	[3] = { job = { section = "near_job", alife_task = task(103, 1003) } },
}, { [3] = true })
local smart_far = make_smart("far", 0, {
	[4] = { job = { section = "far_job", alife_task = task(104, 1004) } },
}, { [4] = true })
local smart_select = make_smart("select", 0, {
	[5] = { test_job = { section = "selected", alife_task = task(105, 1005) } },
})

local smarts = {
	[51] = smart_a,
	[52] = smart_near,
	[53] = smart_far,
	[54] = smart_select,
}

alife = function()
	return {
		object = function(_, smart_id)
			return smarts[smart_id]
		end,
	}
end

smart_terrain.setup_gulag_and_logic_on_spawn = function(npc, _, se_obj)
	local smart = smarts[se_obj.m_smart_terrain_id]
	local info = smart and smart.npc_info[se_obj.id]
	if info and not info.job then
		smart:select_npc_job(info, true)
	end
	if info and info.job then
		smart:setup_logic(npc)
	end
end

local function npc(id)
	local object = { object_id = id, current_position = vec(-1) }
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
		self.current_position = vec(position.x)
	end
	return object
end

xr_motivator = { motivator_binder = {} }
xr_motivator.motivator_binder.net_spawn = function(self, se_obj)
	smart_terrain.setup_gulag_and_logic_on_spawn(self.object, nil, se_obj, 0, true)
	local redirect = db.spawned_vertex_by_id[se_obj.id]
	if redirect then
		-- Active Exo binder performs this coarser overwrite after setup_logic.
		self.object:set_npc_position(level.vertex_position(redirect))
		db.spawned_vertex_by_id[se_obj.id] = nil
	end
	return true
end

dofile(patch_path)
on_game_start()

local function spawn(id, smart_id, server_x)
	local object = npc(id)
	local se_obj = { id = id, m_smart_terrain_id = smart_id, position = vec(server_x) }
	xr_motivator.motivator_binder.net_spawn({ object = object }, se_obj)
	return object
end

local first = spawn(1, 51, 0)
local second = spawn(2, 51, 0)
local near = spawn(3, 52, 5)
local far = spawn(4, 53, 100)
local selected = spawn(5, 54, 0)

assert(setup_positions[1] == 1001, "setup_logic saw centre instead of exact guard position")
assert(setup_positions[2] == 1002, "second NPC was not independently pre-bound")
assert(first:position().x == 1001, "post-spawn binder overwrote exact first position")
assert(second:position().x == 1002, "post-spawn binder overwrote exact second position")
assert(near:position().x == 1003, "near arrival was not treated as already arrived")
assert(far:position().x == -1, "real far arrival was teleported")
assert(selected:position().x == 1005, "unassigned NPC did not receive and use stock job")

print("v106 immediate smart work smoke test: PASS")
