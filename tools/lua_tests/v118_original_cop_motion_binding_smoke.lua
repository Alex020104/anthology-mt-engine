local gamedata = assert(arg[1], "path to v118 gamedata is required")
local script_path = gamedata .. "\\scripts\\zzzzzzzzzzzzz_anthology_cutscenes_v116_root_anchor.script"
local staging_path = gamedata .. "\\scripts\\zzzzzzzzzzzz_anthology_cutscenes_v114_staging.script"

local staging_file = assert(io.open(staging_path, "rb"))
local staging_source = staging_file:read("*a")
staging_file:close()
assert(string.find(staging_source, "return path and path:point(0)", 1, true),
	"v118 staging detached the live luabind patrol point call")
assert(not string.find(staging_source, "pcall(path.point", 1, true),
	"v118 staging retained the detached patrol method call")

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
			if type(x) == "table" then
				self.x = x.x
				self.y = x.y
				self.z = x.z
			else
				self.x = x
				self.y = y
				self.z = z
			end
			return self
		end,
	}
end

local base_calls = 0
state_mgr_animation = {
	animation = {
		add_anim = function(self, motion, state)
			base_calls = base_calls + 1
			local manager = self.mgr
			if manager.animation_position and manager.animation_direction
				and not manager.animation_direction_applied
			then
				manager.animation_direction_applied = true
				self.npc:add_animation(
					motion,
					true,
					manager.animation_position,
					vector():set(0, 0, 0),
					true
				)
			elseif not (state and state.prop and state.prop.moving == true) then
				self.npc:add_animation(motion, true, false)
			else
				self.npc:add_animation(motion, true, true)
			end
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
		add_animation = function(_, motion, looped, position, rotation, local_animation)
			-- Decode the three-argument engine overload used by the base script:
			-- add_animation(motion, hand_usage, use_movement_controller).
			if local_animation == nil and rotation == nil and type(position) == "boolean" then
				local_animation = position
				position = nil
			end
			table.insert(calls, {
				motion = motion,
				looped = looped,
				position = position,
				rotation = rotation,
				local_animation = local_animation,
			})
		end,
	}
	return npc, calls
end

local function make_manager(target_state, x)
	return {
		animation_position = { x = x or 1, y = 2, z = 3 },
		animation_direction = { x = 0, y = 0, z = 1 },
		animation_direction_applied = nil,
		target_state = target_state,
	}
end

local moving_state = { prop = { moving = true } }
local nonmoving_state = { prop = { moving = false } }

local function controller(npc, manager, marker)
	return {
		npc = npc,
		mgr = manager,
		states = { anim_marker = marker or 1 },
	}
end

assert(loadfile(script_path))()
on_game_start()
local wrapped = state_mgr_animation.animation.add_anim
on_game_start()
assert(state_mgr_animation.animation.add_anim == wrapped, "v118 wrapped add_anim twice")

local jup_sections = {
	"jup_b219_actor",
	"jup_b219_stalker_tech",
	"jup_b219_zulus",
	"jup_b219_vano",
	"jup_b219_soldier",
	"jup_b219_monolith_squad_leader_freedom_skin",
}

local pri_sections = {
	"pri_a15_actor",
	"pri_a15_vano",
	"pri_a15_sokolov_scene",
	"pri_a15_zulus",
	"pri_a15_wanderer",
	"pri_a15_military_tarasov",
	"pri_a15_military_2",
	"pri_a15_military_3",
	"pri_a15_military_4",
}

-- Live state_mgr passes this concrete motion, not the logical CoP state name.
current_level = "jupiter"
for index, section in ipairs(jup_sections) do
	local npc, calls = make_npc(index, section)
	local manager = make_manager("pri_a15_idle_none")
	wrapped(controller(npc, manager), "chest_0_idle_0", moving_state)
	assert(#calls == 1 and calls[1].local_animation == false,
		"Jupiter first physical motion was not absolute for " .. section)
	assert(manager.animation_direction_applied == true,
		"Jupiter authored anchor marker was not applied for " .. section)

	-- The remaining authored root-motion chain must still use the base local path.
	wrapped(controller(npc, manager, 3), "jup_b219_descent_actor_1", moving_state)
	assert(#calls == 2 and calls[2].local_animation == true,
		"Jupiter follow-up root motion was not local for " .. section)
end

current_level = "pripyat"
for index, section in ipairs(pri_sections) do
	local npc, calls = make_npc(20 + index, section)
	local manager = make_manager("pri_a15_idle_none")
	wrapped(controller(npc, manager), "chest_0_idle_0", moving_state)
	assert(#calls == 1 and calls[1].local_animation == false,
		"Pripyat first physical motion was not absolute for " .. section)
end

-- A real state/position reset creates a new authored anchor epoch. Logging an
-- NPC once must never suppress the behavior.
local repeat_npc, repeat_calls = make_npc(50, "pri_a15_actor")
local repeat_manager = make_manager("pri_a15_idle_none", 10)
wrapped(controller(repeat_npc, repeat_manager), "chest_0_idle_0", moving_state)
wrapped(controller(repeat_npc, repeat_manager, 3), "pri_a15_igrok_cam1", moving_state)
repeat_manager.animation_position = { x = 20, y = 2, z = 3 }
repeat_manager.animation_direction_applied = nil
repeat_manager.target_state = "pri_a15_actor_all"
wrapped(controller(repeat_npc, repeat_manager), "pri_a15_igrok_cam1", moving_state)
assert(#repeat_calls == 3, "repeat anchor epoch call count mismatch")
assert(repeat_calls[1].local_animation == false, "first repeat-test anchor was not absolute")
assert(repeat_calls[2].local_animation == true, "middle repeat-test motion was not local")
assert(repeat_calls[3].local_animation == false, "second authored anchor epoch was suppressed")
assert(repeat_calls[3].position.x == 20, "second authored anchor used the old position")

-- Retail CoP leaves an explicit authored transform pending across a
-- non-root-moving clip. It is consumed by the first moving physical motion.
local deferred_npc, deferred_calls = make_npc(51, "pri_a15_actor")
local deferred_manager = make_manager("pri_a15_idle_none", 30)
wrapped(controller(deferred_npc, deferred_manager), "guard_0_idle", nonmoving_state)
assert(#deferred_calls == 1 and deferred_calls[1].local_animation == false
	and deferred_calls[1].position == nil,
	"non-moving clip incorrectly created an authored movement controller")
assert(not deferred_manager.animation_direction_applied,
	"non-moving clip incorrectly consumed the authored transform")
wrapped(controller(deferred_npc, deferred_manager), "chest_0_idle_0", moving_state)
assert(#deferred_calls == 2 and deferred_calls[2].local_animation == false
	and deferred_calls[2].position.x == 30,
	"first moving clip did not consume the deferred authored transform")

-- Exact level and exact spawn section keep the patch away from gameplay,
-- Underpass walkers and similarly named addon NPCs.
local base_before_scope_tests = base_calls
current_level = "pripyat"
local unrelated, unrelated_calls = make_npc(60, "sim_default_stalker_1")
wrapped(controller(unrelated, make_manager("pri_a15_idle_none")), "chest_0_idle_0", moving_state)
assert(unrelated_calls[1].local_animation == true, "v118 changed an unrelated Pripyat NPC")

current_level = "zaton"
local wrong_level, wrong_level_calls = make_npc(61, "pri_a15_actor")
wrapped(controller(wrong_level, make_manager("pri_a15_idle_none")), "chest_0_idle_0", moving_state)
assert(wrong_level_calls[1].local_animation == true, "v118 changed a CoP section on the wrong level")

current_level = "jupiter_underground"
local underpass, underpass_calls = make_npc(62, "pas_b400_zulus")
wrapped(controller(underpass, make_manager("pas_b400_zulus_idle")), "chest_0_idle_0", moving_state)
assert(underpass_calls[1].local_animation == true, "v118 changed an Underpass walker")
assert(base_calls == base_before_scope_tests + 3, "scope tests did not delegate exactly three times")

local saw_real_motion = false
local saw_second_epoch = false
for _, line in ipairs(log_lines) do
	if string.find(line, "motion=chest_0_idle_0", 1, true)
		and string.find(line, "state=pri_a15_idle_none", 1, true)
		and string.find(line, "marker=1", 1, true)
	then
		saw_real_motion = true
	end
	if string.find(line, "npc=pri_a15_actor_object", 1, true)
		and string.find(line, "epoch=2", 1, true)
	then
		saw_second_epoch = true
	end
end
assert(saw_real_motion, "v118 diagnostics did not record the live physical motion/logical state pair")
assert(saw_second_epoch, "v118 diagnostics did not record a repeated authored anchor epoch")

print("PASS v118 original CoP motion-binding smoke")
