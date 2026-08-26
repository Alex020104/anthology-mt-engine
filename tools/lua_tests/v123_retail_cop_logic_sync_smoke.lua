local gamedata = assert(arg[1], "path to v123 gamedata is required")
local pri_path = gamedata .. "\\configs\\scripts\\pripyat\\pri_a15_sr_cutscene.ltx"
local jup_path = gamedata .. "\\configs\\scripts\\jupiter\\jup_b219_sr_control.ltx"

local function read_all(path)
	local file = assert(io.open(path, "rb"))
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

local PRI_LIST = "pri_a15_actor:pri_a15_vano:pri_a15_sokolov_scene:pri_a15_zulus:pri_a15_wanderer:pri_a15_military_tarasov:pri_a15_military_2:pri_a15_military_3:pri_a15_military_4"
local JUP_LIST = "jup_b219_actor:jup_b219_stalker_tech_id:jup_b219_zulus_id:jup_b219_vano_id:jup_b219_soldier_id:jup_b219_monolith_squad_leader_freedom_skin_id"

local pri = read_all(pri_path)
local pri_order = "+pri_a15_cutscene_go =update_obj_logic(" .. PRI_LIST .. ") =update_npc_logic(" .. PRI_LIST .. ") =update_obj_logic(pri_a15_exit) =stop_postprocess(3009)"
assert(literal_count(pri, pri_order) == 1,
	"PRI must switch all nine NPCs, pump them, and only then start the camera SR")
assert(literal_count(pri, "=update_obj_logic(" .. PRI_LIST .. ")") == 1,
	"PRI NPC pre-switch must occur exactly once")

local jup = read_all(jup_path)
local jup_pre_switch = "=update_obj_logic(" .. JUP_LIST .. ")"
local jup_pump = "=update_npc_logic(" .. JUP_LIST .. ")"
assert(literal_count(jup, jup_pre_switch) == 16,
	"all eight JUP compositions and fallbacks must pre-switch the cast")
assert(literal_count(jup, jup_pump) == 16,
	"all eight JUP compositions and fallbacks must pump the cast")

local launch_lines = 0
for line in string.gmatch(jup, "[^\r\n]+") do
	if string.find(line, "sr_cutscene@start", 1, true)
		and string.find(line, "jup_b219_start_", 1, true)
		and string.find(line, "=stop_postprocess(22219)", 1, true)
	then
		launch_lines = launch_lines + 1
		local start_pos = assert(string.find(line, "+jup_b219_start_", 1, true))
		local switch_pos = assert(string.find(line, jup_pre_switch, 1, true))
		local pump_pos = assert(string.find(line, jup_pump, 1, true))
		local reveal_pos = assert(string.find(line, "=stop_postprocess(22219)", 1, true))
		assert(start_pos < switch_pos and switch_pos < pump_pos and pump_pos < reveal_pos,
			"JUP launch order must be info -> switch -> planner pump -> reveal/camera")
	end
end
assert(launch_lines == 16, "unexpected number of JUP launch branches")

print("v123 retail CoP LTX logic sync smoke test passed")
