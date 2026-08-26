local root = assert(arg[1], "engine repository path is required")

local function read_all(relative)
	local path = root .. "\\" .. relative
	local file = assert(io.open(path, "rb"), "cannot open " .. path)
	local value = file:read("*a")
	file:close()
	return value
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

local threading = read_all("src\\xrEngine\\EngineThreading.cpp")
require_literal(threading, '"pri_a15_"', "PRI cinematic actor scope is missing")
require_literal(threading, '"jup_b219_"', "JUP cinematic actor scope is missing")
require_literal(threading, '"pas_b400_"', "PAS cinematic actor scope is missing")
require_literal(threading, "original_cop_cinematic_controllers.fetch_add",
	"cinematic controller registration is missing")
require_literal(threading, "original_cop_cinematic_controllers.compare_exchange_weak",
	"balanced cinematic controller unregister is missing")
require_literal(threading, "original_cop_cinematic_grace_until_frame.store(Device.dwFrame + 1",
	"the destroy-to-next-clip boundary frame must remain serialized")
require_literal(threading, "s32(grace_until - Device.dwFrame) >= 0",
	"the cinematic serialization grace window is not consumed")

local game_object = read_all("src\\xrGame\\GameObject.cpp")
require_order(game_object, {
	"RegisterOriginalCoPCinematicController()",
	"xr_new<animation_movement_controller>"
}, "the synchronization guard must be active before the root controller is constructed")
require_order(game_object, {
	"void CGameObject::destroy_anim_mov_ctrl()",
	"UnregisterOriginalCoPCinematicController()",
	"xr_delete(m_anim_mov_ctrl)"
}, "controller destruction must release the synchronization guard exactly with ownership")

local device = read_all("src\\xrEngine\\device.cpp")
require_order(device, {
	"serialize_original_cop_cinematic",
	"secondary_tasks.wait();",
	"XRay::Engine::GameThread();",
	"XRay::Engine::CalculateBonesThread();"
}, "cinematic frame order must be worker barrier -> GameThread -> bones")
require_literal(device, "secondary_tasks.run(&XRay::Engine::GameThread);",
	"normal gameplay must retain the asynchronous GameThread path")

local ik = read_all("src\\xrGame\\IKLimbsController.cpp")
local callback_first = assert(string.find(ik, "CIKLimbsController::IKVisualCallback", 1, true))
local callback_last = assert(string.find(ik, "void CIKLimbsController::PlayLegs", callback_first, true))
local callback = string.sub(ik, callback_first, callback_last - 1)
require_order(callback, {
	"m_original_cop_update_frame == Device.dwFrame",
	"ik->Calculate();",
	"return;",
	"Render->ViewBase.testSphere_dirty"
}, "retail cinematic callback must calculate before all Monolith culling")
require_literal(callback, "root_bi.reset_callback();",
	"ordinary NPCs must retain the existing optimized callback path")

local update_first = assert(string.find(ik, "void CIKLimbsController::Update()", 1, true))
local update = string.sub(ik, update_first)
require_order(update, {
	"skeleton_animated->UpdateTracks();",
	"_pose_extrapolation.update(m_object->XFORM());",
	"LimbUpdate(limb);",
	"m_original_cop_update_frame = Device.dwFrame;"
}, "retail pose preparation must run after tracks")
assert(not string.find(update, "CalculateBones", 1, true),
	"cinematic IK Update must not preempt the post-GameThread bones pass")

local physics = read_all("src\\xrGame\\CharacterPhysicsSupport.cpp")
require_order(physics, {
	"UsesOriginalCoPCinematicPipeline()",
	"update_interactive_anims();",
	"ik_controller()->Update();",
	"Device.CalcSSADynamic"
}, "active cinematic actors must bypass SSA/frustum update suppression")

print("v124 retail CoP IK pipeline smoke test passed")
