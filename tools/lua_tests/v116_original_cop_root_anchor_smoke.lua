local gamedata = assert(arg[1], "path to v116 gamedata is required")
local script_path = gamedata .. "\\scripts\\zzzzzzzzzzzzz_anthology_cutscenes_v116_root_anchor.script"

local current_level = "jupiter"
level = {
	name = function()
		return current_level
	end,
}

local log_lines = {}
function printf(pattern, ...)
	table.insert(log_lines, string.format(pattern, ...))
end

function CreateTimeEvent()
	error("state_mgr_animation must be available without a retry in this smoke test")
end

function vector()
	return {
		set = function(self, x, y, z)
			self.x = x
			self.y = y
			self.z = z
			return self
		end,
	}
end

local base_calls = 0
state_mgr_animation = {
	animation = {
		add_anim = function(self, animation_name)
			base_calls = base_calls + 1
			self.npc:add_animation(animation_name, true, self.mgr.animation_position,
				vector():set(0, 0, 0), true)
		end,
	},
}

local function make_npc(id, section)
	local calls = {}
	local npc = {
		id = function()
			return id
		end,
		name = function()
			return section .. "_object"
		end,
		section = function()
			return section
		end,
		add_animation = function(_, animation_name, looped, position, rotation, local_animation)
			table.insert(calls, {
				animation = animation_name,
				looped = looped,
				position = position,
				rotation = rotation,
				local_animation = local_animation,
			})
		end,
	}
	return npc, calls
end

local function manager()
	return {
		animation_position = { x = 1, y = 2, z = 3 },
		animation_direction = { x = 0, y = 0, z = 1 },
		animation_direction_applied = nil,
	}
end

assert(loadfile(script_path))()
on_game_start()
local wrapped = state_mgr_animation.animation.add_anim
on_game_start()
assert(state_mgr_animation.animation.add_anim == wrapped, "v116 wrapped add_anim twice")

local jup_actor, jup_calls = make_npc(1, "jup_b219_actor")
local jup_state = manager()
local moving_state = { prop = { moving = true } }
wrapped({ npc = jup_actor, mgr = jup_state }, "pri_a15_idle_none", moving_state)
assert(#jup_calls == 1 and jup_calls[1].local_animation == false,
	"Jupiter authored idle anchor was not restored to original CoP absolute mode")
assert(jup_state.animation_direction_applied == true, "Jupiter anchor did not preserve the applied marker")

-- Once the anchor is applied, the authored moving chain must stay local.
wrapped({ npc = jup_actor, mgr = jup_state }, "jup_b219_descent_actor_1", moving_state)
assert(#jup_calls == 2 and jup_calls[2].local_animation == true,
	"Jupiter moving root-motion chain was made absolute")

current_level = "pripyat"
local pri_npc, pri_calls = make_npc(2, "pri_a15_sokolov_scene")
wrapped({ npc = pri_npc, mgr = manager() }, "pri_a15_sokolov_1", moving_state)
assert(#pri_calls == 1 and pri_calls[1].local_animation == false,
	"Pripyat authored root anchor was not restored to original CoP absolute mode")

local unrelated, unrelated_calls = make_npc(3, "sim_default_stalker_1")
wrapped({ npc = unrelated, mgr = manager() }, "pri_a15_idle_none", {})
assert(#unrelated_calls == 1 and unrelated_calls[1].local_animation == true,
	"v116 changed an unrelated NPC anchor")

current_level = "jupiter_underground"
local pas_npc, pas_calls = make_npc(4, "pas_b400_zulus")
wrapped({ npc = pas_npc, mgr = manager() }, "pas_b400_zulus_idle", {})
assert(#pas_calls == 1 and pas_calls[1].local_animation == true,
	"v116 changed an Underpass gameplay walker/remark animation")

assert(base_calls == 3, "unexpected delegation count: " .. tostring(base_calls))
assert(#log_lines >= 3, "v116 diagnostics were not emitted")

print("PASS v116 original CoP root anchor smoke")
