local patch_path = assert(arg[1], "path to v105 pre-bind script is required")

db = {
	offline_objects = {
		[1] = { level_vertex_id = 10 },
		[2] = { level_vertex_id = 10 },
		[3] = { level_vertex_id = 303 },
		[4] = { level_vertex_id = 20 },
	},
	spawned_vertex_by_id = {},
}

level = {
	vertex_position = function(level_vertex)
		return { level_vertex = level_vertex }
	end,
}

printf = function()
end

local function task(level_vertex)
	return {
		level_vertex_id = function()
			return level_vertex
		end,
	}
end

local smart_a = {
	npc_info = {
		[1] = { job = { alife_task = task(101) } },
		[2] = { job = { alife_task = task(102) } },
	},
	arriving_npc = {},
}

local smart_b = {
	npc_info = {
		[3] = { job = { alife_task = task(103) } },
	},
	arriving_npc = {},
}

local smart_c = {
	npc_info = {
		[4] = { job = { alife_task = task(104) } },
	},
	arriving_npc = { [4] = true },
}

local smarts = {
	[55] = smart_a,
	[56] = smart_b,
	[57] = smart_c,
}

alife = function()
	return {
		object = function(_, smart_id)
			return smarts[smart_id]
		end,
	}
end

local positions_seen_by_setup = {}
smart_terrain = {
	setup_gulag_and_logic_on_spawn = function(npc, _, se_obj)
		positions_seen_by_setup[se_obj.id] = npc.level_vertex
		db.offline_objects[se_obj.id] = {}
		return true
	end,
}

dofile(patch_path)
on_game_start()

local function npc()
	return {
		set_npc_position = function(self, position)
			self.level_vertex = position.level_vertex
		end,
	}
end

smart_terrain.setup_gulag_and_logic_on_spawn(npc(), nil, { id = 1, m_smart_terrain_id = 55 })
smart_terrain.setup_gulag_and_logic_on_spawn(npc(), nil, { id = 2, m_smart_terrain_id = 55 })
smart_terrain.setup_gulag_and_logic_on_spawn(npc(), nil, { id = 3, m_smart_terrain_id = 56 })
smart_terrain.setup_gulag_and_logic_on_spawn(npc(), nil, { id = 4, m_smart_terrain_id = 57 })

assert(positions_seen_by_setup[1] == 101, "first collapsed NPC was not pre-bound")
assert(positions_seen_by_setup[2] == 102, "snapshot did not survive sequential offline cleanup")
assert(positions_seen_by_setup[3] == 303, "unique saved position was not preserved")
assert(positions_seen_by_setup[4] == nil, "arriving NPC was moved")
assert(db.spawned_vertex_by_id[1] == 101, "first stock redirect was not seeded")
assert(db.spawned_vertex_by_id[2] == 102, "second stock redirect was not seeded")

print("v105 pre-bound smart jobs smoke test: PASS")
