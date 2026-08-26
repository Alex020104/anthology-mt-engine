local root = assert(arg[1], "engine repository path is required")

local function read_all(relative)
	local path = root .. "\\" .. relative
	local file = assert(io.open(path, "rb"), "cannot open " .. path)
	local value = file:read("*a")
	file:close()
	return value
end

local function literal_count(source, needle)
	local count = 0
	local offset = 1
	while true do
		local first, last = string.find(source, needle, offset, true)
		if not first then
			return count
		end
		count = count + 1
		offset = last + 1
	end
end

local function require_literal(source, needle, message)
	assert(string.find(source, needle, 1, true), message or ("missing: " .. needle))
end

local function require_order(source, needles, message)
	local offset = 1
	for _, needle in ipairs(needles) do
		local position = string.find(source, needle, offset, true)
		assert(position, message or ("missing or out of order: " .. needle))
		offset = position + #needle
	end
end

local game_object = read_all("src\\xrGame\\GameObject.cpp")
require_literal(game_object, '"pri_a15_"', "PRI cadence scope is missing")
require_literal(game_object, '"jup_b219_"', "JUP cadence scope is missing")
require_literal(game_object, '"pas_b400_"', "PAS cadence scope is missing")
require_order(game_object, {
	"m_original_cop_cinematic_cadence = true;",
	"[cop-cadence] arm",
	"xr_new<animation_movement_controller>"
}, "cadence must be armed before the movement controller calculates its first pose")
local cadence_first = assert(string.find(game_object,
	"bool CGameObject::original_cop_cinematic_cadence_active() const", 1, true))
local cadence_last = assert(string.find(game_object,
	"void CGameObject::note_original_cop_ik_prepare()", cadence_first, true))
local cadence_predicate = string.sub(game_object, cadence_first, cadence_last - 1)
require_literal(cadence_predicate, "return m_original_cop_cinematic_cadence;",
	"constructor-time cadence arm must not depend on an assigned controller pointer")
assert(not string.find(cadence_predicate, "animation_movement_controlled()", 1, true),
	"constructor-time cadence arm cannot query m_anim_mov_ctrl before xr_new returns")
require_order(game_object, {
	"m_cop_cadence_last_root_frame = Device.dwFrame;",
	"++m_cop_cadence_root_updates;",
	"m_anim_mov_ctrl->OnFrame();"
}, "root cadence must be sampled immediately before root-motion advances")
require_literal(game_object, "[cop-cadence] root-update-gap",
	"root update gaps are not diagnosable")
require_literal(game_object, "[cop-cadence] disarm",
	"the per-controller cadence summary is missing")

local physics = read_all("src\\xrGame\\CharacterPhysicsSupport.cpp")
require_order(physics, {
	"UsesOriginalCoPCinematicPipeline()",
	"update_interactive_anims();",
	"PrepareOriginalCoPCinematicFrame();",
	"Device.CalcSSADynamic"
}, "cinematic pose preparation must bypass ordinary NPC culling")
require_literal(physics, "FinalizeOriginalCoPCinematicFrame();",
	"physics support does not expose the end-of-stalker finalize step")

local ik = read_all("src\\xrGame\\IKLimbsController.cpp")
local callback_first = assert(string.find(ik, "CIKLimbsController::IKVisualCallback", 1, true))
local callback_last = assert(string.find(ik, "void CIKLimbsController::PlayLegs", callback_first, true))
local callback = string.sub(ik, callback_first, callback_last - 1)
require_order(callback, {
	"m_original_cop_force_calculate",
	"ik->Calculate();",
	"note_original_cop_bone_calculation();",
	"return;",
	"Render->ViewBase.testSphere_dirty"
}, "cinematic IK calculation must precede all ordinary NPC culling")
assert(literal_count(callback, "!O->original_cop_cinematic_cadence_active()") == 2,
	"both distance and frustum cullers must preserve root callbacks during constructor handoff")

local prepare_first = assert(string.find(ik, "void CIKLimbsController::PrepareOriginalCoPCinematicFrame()", 1, true))
local prepare_last = assert(string.find(ik, "void CIKLimbsController::LimbSetup", prepare_first, true))
local prepare = string.sub(ik, prepare_first, prepare_last - 1)
require_order(prepare, {
	"Update();",
	"_pose_extrapolation.update(m_object->XFORM());",
	"LimbUpdate(limb);",
	"m_original_cop_prepared_frame = Device.dwFrame;",
	"note_original_cop_ik_prepare();"
}, "retail CoP pose preparation order is incomplete")
require_order(prepare, {
	"m_original_cop_force_calculate = true;",
	"K->CalculateBones_Invalidate();",
	"K->CalculateBones(TRUE);",
	"m_original_cop_force_calculate = false;",
	"original_cop_bones_calculated_this_frame()"
}, "exact bones must be finalized inside the short callback-bypass window")

local stalker = read_all("src\\xrGame\\ai\\stalker\\ai_stalker.cpp")
require_order(stalker, {
	"BOOL CAI_Stalker::AlwaysTheCrow()",
	"interactive_motion() ||",
	"original_cop_cinematic_cadence_active()"
}, "active authored movement must keep the NPC in every-frame UpdateCL")
require_order(stalker, {
	"g_mt_config.test(mtObjectHandler)",
	"!is_original_cop_cinematic_actor()",
	"Device.add_to_seq_parallel"
}, "cinematic object-handler mutations must remain on the main object-update path")
require_order(stalker, {
	"weapon_shot_effector().Update();",
	"finalize_original_cop_cinematic_frame();",
	"debug_text"
}, "exact cinematic bones must be committed after all stalker frame writers")

local device = read_all("src\\xrEngine\\device.cpp")
assert(not string.find(device, "OriginalCoPCinematicControllerActive", 1, true),
	"v124 global game/bones serialization must not return")

local patch_root = "modpack-patches\\Anthology Cutscenes v125 - Continuous CoP Root Cadence\\gamedata\\configs\\scripts\\"
local jup_ltx = read_all(patch_root .. "jupiter\\jup_b219_sr_control.ltx")
local pri_ltx = read_all(patch_root .. "pripyat\\pri_a15_sr_cutscene.ltx")
local jup_cast = "jup_b219_actor:jup_b219_stalker_tech_id:jup_b219_zulus_id:jup_b219_vano_id:jup_b219_soldier_id:jup_b219_monolith_squad_leader_freedom_skin_id"
local pri_cast = "pri_a15_actor:pri_a15_vano:pri_a15_sokolov_scene:pri_a15_zulus:pri_a15_wanderer:pri_a15_military_tarasov:pri_a15_military_2:pri_a15_military_3:pri_a15_military_4"

assert(literal_count(jup_ltx, "=update_npc_logic(") == 0,
	"JUP must not pump planner/state manager outside the normal NPC tick")
assert(literal_count(pri_ltx, "=update_npc_logic(") == 0,
	"PRI must not pump planner/state manager outside the normal NPC tick")
assert(literal_count(jup_ltx, "=update_obj_logic(" .. jup_cast .. ")") == 16,
	"all JUP normal/fallback branches must still switch the cinematic cast")
assert(literal_count(pri_ltx, "=update_obj_logic(" .. pri_cast .. ")") == 1,
	"PRI must still switch all nine cinematic actors before starting its camera SR")

print("v125 continuous CoP root cadence smoke test passed")
