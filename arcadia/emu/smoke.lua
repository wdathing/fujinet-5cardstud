-- smoke.lua: headless smoke test of the whole flow. The cart boots
-- straight into the name screen (no OS menu on the Arcadia): Enter
-- accepts the ARCADIA default, Enter joins the selected table (row 0 =
-- the AI room), then keypad '2' every 3 seconds -- in the game that wire
-- is strictly "move 2", validated against the server's menu, and the AI
-- bots keep real move windows coming. Near the end it checks that the
-- cart's ACKSEQ shows a long transaction history and snapshots.
--   FUJINET_DEBUG=1 mame arcadia -cartslot fujinet -cart build/5card.bin \
--       -autoboot_script emu/smoke.lua -video none -sound none \
--       -seconds_to_run 70 -snapshot_directory build
--
-- Keypad ioports (arcadia.cpp): ':controller1_col3' 0x01 = Enter,
-- ':controller1_col2' 0x08 = '2'/FIRE.

local phase = 0
local shot = false
local key2 = false
local verdict = nil

emu.register_frame(function()
    local t = manager.machine.time.seconds
    local col2 = manager.machine.ioport.ports[":controller1_col2"]
    local col3 = manager.machine.ioport.ports[":controller1_col3"]
    if col2 == nil or col3 == nil then return end
    if phase == 0 and t >= 3 then
        emu.print_info("smoke: Enter (name) t=" .. t)
        col3:field(0x01):set_value(1); phase = 1
    elseif phase == 1 and t >= 4 then
        col3:field(0x01):set_value(0); phase = 2
    elseif phase == 2 and t >= 8 then
        emu.print_info("smoke: Enter (join) t=" .. t)
        col3:field(0x01):set_value(1); phase = 3
    elseif phase == 3 and t >= 9 then
        col3:field(0x01):set_value(0); phase = 4
    elseif phase == 4 and t >= 12 then
        local m = t % 3
        if m < 1 and not key2 then
            col2:field(0x08):set_value(1); key2 = true
        elseif m >= 1 and key2 then
            col2:field(0x08):set_value(0); key2 = false
        end
    end
    if not shot and t >= 64 then
        local mem = manager.machine.devices[":maincpu"].spaces["program"]
        local seq = mem:read_u8(0x2C00)
        if seq >= 25 then verdict = "PASS (ackseq " .. seq .. ")"
        else verdict = "FAIL (ackseq only " .. seq .. ")" end
        emu.print_info("smoke: " .. verdict)
        manager.machine.video:snapshot()
        shot = true
    elseif t >= 66 then
        print("smoke: " .. (verdict or "FAIL (never checked)"))
        manager.machine:exit()
    end
end)
