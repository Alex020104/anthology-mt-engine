local addon_root = assert(arg[1], "path to v110 gamedata is required")
local scripts = addon_root .. "\\scripts\\"

-- Sound-theme bypass: cinematic themes must use the original npc_sound path,
-- while an ordinary managed theme must remain handled by the death-cleanup
-- patch rather than being globally reverted.
local original_sound_calls = 0
npc_sound = {
	play = function()
		original_sound_calls = original_sound_calls + 1
		return true
	end,
	stop = function() end,
	reset = function() end,
	is_playing = function() return false end,
}
xr_effects = {
	play_sound = function()
		return npc_sound:play(1)
	end,
}
xr_sound = { stop_sounds_by_id = function() end }
RegisterScriptCallback = function() end
printf = function() end

dofile(scripts .. "npc_dialog_sound_kill_fix.script")
on_game_start()

xr_effects.play_sound(nil, nil, { "jup_b219_test_line" })
xr_effects.play_sound(nil, nil, { "pas_b400_test_line" })
xr_effects.play_sound(nil, nil, { "pri_a15_test_line" })
assert(original_sound_calls == 3, "CoP cinematic themes did not keep stock npc_sound playback")

xr_effects.play_sound(nil, nil, { "ordinary_managed_line" })
assert(original_sound_calls == 3, "ordinary dialogue unexpectedly bypassed managed playback")

-- Camera bypass: the two compound CoP camera sets must go straight to the
-- sr_cutscene originals without bridge holds, effector removal or teleport
-- substitution.
local starts, stops, enters, callbacks, removed = 0, 0, 0, 0, 0
cam_effector_set = {
	start_effect = function() starts = starts + 1 end,
	stop_effect = function() stops = stops + 1 end,
}
action_cutscene = {
	zone_enter = function() enters = enters + 1 end,
	cutscene_callback = function() callbacks = callbacks + 1 end,
}
level = {
	remove_cam_effector = function() removed = removed + 1 end,
}
CreateTimeEvent = function() end

dofile(scripts .. "zz_cutscene_smooth_bridge.script")
on_game_start()

for _, camera_set in ipairs({ "jup_b219_descent_camera", "pri_a15_cameffector" }) do
	local st = { cam_effector = { camera_set } }
	cam_effector_set.start_effect({ st = st }, { global_cameffect = true })
	cam_effector_set.stop_effect({ st = st })
	action_cutscene.zone_enter({ st = st })
	action_cutscene.cutscene_callback({ st = st })
end

assert(starts == 2 and stops == 2, "strict CoP camera did not call original cam methods")
assert(enters == 2 and callbacks == 2, "strict CoP action did not call original scene methods")
assert(removed == 0, "camera bridge removed an authored CoP effector")

print("v110 CoP cinematic synchronization smoke test: PASS")
