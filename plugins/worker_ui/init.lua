local exports = {}
exports.name = "workerui"
exports.version = "0.0.1"
exports.description = "worker-ui plugin"
exports.license = "The BSD 3-Clause License"
exports.author = { name = "Bletch" }

local console = exports

function quoted_string_split(text)
	local result = {}
	local e = 0
	local i = 1
	while true do
		local b = e+1
		b = text:find("%S",b)
		if b==nil then break end
		if text:sub(b,b)=="'" then
			e = text:find("'",b+1)
			b = b+1
		elseif text:sub(b,b)=='"' then
			e = text:find('"',b+1)
			b = b+1
		else
			e = text:find("%s",b+1)
		end
		if e==nil then e=#text+1 end

		result[i] = text:sub(b,e-1)
		i = i + 1
	end
	return result
end

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end

function toboolean(str)
	str = str:lower()
	return str == "1" or str == "on" or str == "true"
end

function string_from_bool(b)
	if b then
		return "1"
	else
		return "0"
	end
end

function xml_encode(str)
	local res = ""
	local seq = 0
	local val = nil
	for i = 1, #str do
		-- decode this byte
		local c = string.byte(str, i)
		if seq == 0 then	
			seq = c < 0x80 and 1 or c < 0xE0 and 2 or c < 0xF0 and 3 or
			      c < 0xF8 and 4 or -1
			val = bit32.band(c, 2^(8-seq) - 1)
		else
			val = bit32.bor(bit32.lshift(val, 6), bit32.band(c, 0x3F))
		end

		-- do we have an invalid sequence?  if so, serve up a '?'
		if seq < 0 then
			val = 63
			seq = 1
		end

		-- emit the character if appropriate
		seq = seq - 1
		if seq == 0 then
			if val > 127 then
				res = res .. "&#" .. tostring(val) .. ";"
			elseif string.char(val) == "\"" then
				res = res .. "&quot;"
			elseif string.char(val) == "&" then
				res = res .. "&amp;"
			elseif string.char(val) == "<" then
				res = res .. "&lt;"
			elseif string.char(val) == ">" then
				res = res .. "&gt;"
			else
				res = res .. string.char(val)
			end
			val = 0
		end
	end
	return res
end

function get_images()
	local result = {}
	local i = 1

	for k,v in pairs(manager:machine().images) do
		if k == v.instance_name then
			result[i] = v
			i = i + 1
		end
	end
	return result
end

function find_image_by_tag(tag)
	if not (tag.sub(1, 1) == ":") then
		tag = ":" .. tag
	end

	for _,image in ipairs(get_images()) do
		if image.device:tag() == tag then
			return image
		end
	end
end

function find_port_and_field(tag, mask)
	if not (tag.sub(1, 1) == ":") then
		tag = ":" .. tag
	end

	local port = manager:machine():ioport().ports[tag]
	if not port then
		return
	end

	for k,v in pairs(port.fields) do
		if v.mask == tonumber(mask) then
			return v
		end
	end
end

-- input polling
local current_poll_field
local current_poll_seq_type

function is_polling_input_seq()
	if current_poll_field then
		return true
	else
		return false
	end
end

function stop_polling_input_seq()
	current_poll_field = nil
	current_poll_seq_type = nil
	manager:ui():set_aggressive_input_focus(false)
end

function emit_status(light)
	if light == nil then
		light = false
	end

	print("<status");
	print("\tphase=\"running\"");
	print("\tpolling_input_seq=\"" .. tostring(is_polling_input_seq()) .. "\"");
	print("\tnatural_keyboard_in_use=\"" .. tostring(manager:machine():ioport():natkeyboard().in_use) .. "\"");
	print("\tpaused=\"" .. tostring(manager:machine().paused) .. "\"");
	print("\tstartup_text=\"\"");
	print("\tshow_profiler=\"" .. tostring(manager:ui().show_profiler) .. "\"");
	print(">");

	-- <video> (video_manager)
	print("\t<video");
	print("\t\tspeed_percent=\"" .. tostring(manager:machine():video():speed_percent()) .. "\"");
	print("\t\tframeskip=\"" .. tostring(manager:machine():video().frameskip) .. "\"");
	print("\t\teffective_frameskip=\"" .. tostring(manager:machine():video():effective_frameskip()) .. "\"");
	print("\t\tthrottled=\"" .. tostring(manager:machine():video().throttled) .. "\"");
	print("\t\tthrottle_rate=\"" .. tostring(manager:machine():video().throttle_rate) .. "\"");
	print("\t/>");

	-- <sound> (sound_manager)
	print("\t<sound");
	print("\t\tattenuation=\"" .. tostring(manager:machine():sound().attenuation) .. "\"");
	print("\t/>");

	if (not light or manager:machine().paused or is_polling_input_seq()) then
		-- <images>
		print("\t<images>")
		for _,image in ipairs(get_images()) do

			local filename = image:filename()
			if filename == nil then
				filename = ""
			end

			-- basic image properties
			print(string.format("\t\t<image tag=\"%s\" instance_name=\"%s\" is_readable=\"%s\" is_writeable=\"%s\" is_creatable=\"%s\" must_be_loaded=\"%s\"",
				xml_encode(image.device:tag()),
				xml_encode(image.instance_name),
				string_from_bool(image.is_readable),
				string_from_bool(image.is_writeable),
				string_from_bool(image.is_creatable),
				string_from_bool(image.must_be_loaded)))

			-- filename
			local filename = image:filename()
			if filename ~= nil and filename ~= "" then
				print("\t\t\tfilename=\"" .. xml_encode(filename) .. "\"")
			end

			-- display
			local display = image:display()
			if display ~= nil and display ~= "" then
				print("\t\t\tdisplay=\"" .. xml_encode(display) .. "\"")
			end

			print("\t\t/>")
		end	
		print("\t</images>")

		-- <inputs>
		print("\t<inputs>")
		for _,port in pairs(manager:machine():ioport().ports) do
			for _,field in pairs(port.fields) do
				if field.enabled then
					local type_class = field.type_class
					local is_switch = type_class == "dipswitch" or type_class == "config"
					local field_type
					if field.is_analog then
						field_type = "analog"
					else
						field_type = "digital"
					end

					print("\t\t<input"
						.. " port_tag=\"" .. xml_encode(port:tag()) .. "\""
						.. " mask=\"" .. tostring(field.mask) .. "\""
						.. " class=\"" .. type_class .. "\""
						.. " type=\"" .. field_type .. "\""
						.. " name=\"" .. xml_encode(field.name) .. "\"")

					if is_switch then
						-- DIP switches and configs have values
						print("\t\t\tvalue=\"" .. tostring(field.user_value) .. "\"")
					end

					print("\t\t>")

					-- emit input sequences for anything that is not DIP switches of configs
					if not is_switch then
						-- both analog and digital have "standard" seq types
						print("\t\t\t<seq type=\"standard\" text=\"" .. xml_encode(manager:machine():input():seq_name(field:input_seq("standard"))) .. "\"/>")
						if field.is_analog then
							-- analog inputs also have increment and decrement
							print("\t\t\t<seq type=\"increment\" text=\"" .. xml_encode(manager:machine():input():seq_name(field:input_seq("increment"))) .. "\"/>")
							print("\t\t\t<seq type=\"decrement\" text=\"" .. xml_encode(manager:machine():input():seq_name(field:input_seq("decrement"))) .. "\"/>")						
						end
					end

					print("\t\t</input>")
				end
			end
		end
		print("\t</inputs>")
	end

	print("</status>");
end

-- EXIT command
function command_exit(args)
	manager:machine():exit()
	print "OK ### Exit scheduled"
end

-- PING command
function command_ping(args)
	print "OK STATUS ### Ping... pong..."
	emit_status(true)
end

-- SOFT_RESET command
function command_soft_reset(args)
	manager:machine():soft_reset()
	print("OK ### Soft Reset Scheduled")
end

-- HARD_RESET command
function command_hard_reset(args)
	manager:machine():hard_reset()
	print("OK ### Hard Reset Scheduled")
end

-- PAUSE command
function command_pause(args)
	emu.pause()
	print("OK STATUS ### Paused")
	emit_status()
end

-- RESUME command
function command_resume(args)
	emu.unpause()
	print("OK STATUS ### Resumed")
	emit_status()
end

-- THROTTLED command
function command_throttled(args)
	manager:machine():video().throttled = toboolean(args[2])
	print("OK STATUS ### Throttled set to " .. tostring(manager:machine():video().throttled))
	emit_status()
end

-- THROTTLE_RATE command
function command_throttle_rate(args)
	manager:machine():video().throttle_rate = tonumber(args[2])
	print("OK STATUS ### Throttle rate set to " .. tostring(manager:machine():video().throttle_rate))
	emit_status()
end

-- FRAMESKIP command
function command_frameskip(args)
	local frameskip
	if (args[2]:lower() == "auto") then
		frameskip = -1
	else
		frameskip = tonumber(args[2])
	end
	manager:machine():video().frameskip = frameskip
	print("OK STATUS ### Frame skip rate set to " .. tostring(manager:machine():video().frameskip))
	emit_status()
end

-- INPUT command
function command_input(args)
	manager:machine():ioport():natkeyboard():post(args[2])
	print("OK ### Text inputted")
end

-- PASTE command
function command_paste(args)
	manager:machine():ioport():natkeyboard():paste(args[2])
	print("OK ### Text inputted from clipboard")
end

-- SET_ATTENUATION command
function command_set_attenuation(args)
	manager:machine():sound().attenuation = tonumber(args[2])
	print("OK STATUS ### Sound attenuation set to " .. tostring(manager:machine():sound().attenuation))
	emit_status()
end

-- SET_NATURAL_KEYBOARD_IN_USE command
function command_set_natural_keyboard_in_use(args)
	manager:machine():ioport():natkeyboard().in_use = toboolean(args[2])
	print("OK STATUS ### Natural keyboard in use set to " .. tostring(manager:machine():ioport():natkeyboard().in_use))
	emit_status()
end

-- STATE_LOAD command
function command_state_load(args)
	manager:machine():load(args[2])
	print("OK ### Scheduled state load of '" .. args[2] .. "'")
end

-- STATE_SAVE command
function command_state_save(args)
	manager:machine():save(args[2])
	print("OK ### Scheduled state save of '" .. args[2] .. "'")
end

-- SAVE_SNAPSHOT command
function command_save_snapshot(args)
	-- hardcoded for first screen now
	local index = tonumber(args[2])
	for k,v in pairs(manager:machine().screens) do
		if index == 0 then
			screen = v
			break
		end
		index = index - 1
	end

	screen:snapshot(args[3])
	print("OK ### Successfully saved screenshot '" .. args[3] .. "'")
end

-- LOAD command
function command_load(args)
	local image = find_image_by_tag(args[2])
	if not image then
		print("ERROR ### Cannot find device '" .. args[2] .. "'")
		return
	end
	image:load(args[3])
	print("OK STATUS ### Device '" .. args[2] .. "' loaded '" .. args[3] .. "' successfully")
	emit_status()
end

-- UNLOAD command
function command_unload(args)
	local image = find_image_by_tag(args[2])
	if not image then
		print("ERROR ### Cannot find device '" .. args[2] .. "'")
		return
	end
	image:unload()
	print("OK STATUS ### Device '" .. args[2] .. "' unloaded successfully")
	emit_status()
end

-- CREATE command
function command_create(args)
	local image = find_image_by_tag(args[2])
	if not image then
		print("ERROR ### Cannot find device '" .. args[2] .. "'")
		return
	end
	image:create(args[3])
	print("OK STATUS ### Device '" .. args[2] .. "' created image '" .. args[3] .. "' successfully")
	emit_status()
end

-- SEQ_POLL_START command
function command_seq_poll_start(args)
	local field = find_port_and_field(args[2], args[3])
	if not field then
		print("ERROR ### Can't find field mask '" .. tostring(tonumber(args[3])) .. "' on port '" .. args[2] .. "'")
		return
	end
	if not field.enabled then
		print("ERROR ### Field '" .. args[2] .. "':" .. tostring(tonumber(args[3])) .. " is disabled")
		return
	end

	local input_seq_class
	if field.is_analog then
		input_seq_class = "absolute"
	else
		input_seq_class = "switch"
	end

	manager:machine():input():seq_poll_start(input_seq_class)
	manager:ui():set_aggressive_input_focus(true)
	current_poll_field = field
	current_poll_seq_type = args[4]
	print("OK STATUS ### Starting polling")
	emit_status()
end

-- SEQ_POLL_STOP command
function command_seq_poll_stop(args)
	stop_polling_input_seq()
	print("OK STATUS ### Stopped polling");
	emit_status()
end

-- SET_INPUT_VALUE command
function command_set_input_value(args)
	local field = find_port_and_field(args[2], args[3])
	if not field then
		print("ERROR ### Can't find field mask '" .. tostring(tonumber(args[3])) .. "' on port '" .. args[2] .. "'")
		return
	end
	if not field.enabled then
		print("ERROR ### Field '" .. args[2] .. "':" .. tostring(tonumber(args[3])) .. " is disabled")
		return
	end

	field.user_value = tonumber(args[4]);
	print("OK STATUS ### Field '" .. args[2] .. "':" .. tostring(args[3]) .. " set to " .. tostring(field.user_value))
	emit_status()
end

-- SHOW_PROFILER Command
function command_show_profiler(args)
	manager:ui().show_profiler = toboolean(args[2])
	print("OK STATUS ### Show profiler set to " .. tostring(manager:ui().show_profiler))
	emit_status()
end

-- not implemented command
function command_nyi(args)
	print("ERROR ### Command '" .. args[1] .. "' not yet implemeted")
end

-- unknown command
function command_unknown(args)
	print("ERROR ### Unrecognized command '" .. args[1] .. "'")
end

-- command list
local commands =
{
	["exit"]						= command_exit,
	["ping"]						= command_ping,
	["soft_reset"]					= command_soft_reset,
	["hard_reset"]					= command_hard_reset,
	["throttled"]					= command_throttled,
	["throttle_rate"]				= command_throttle_rate,
	["frameskip"]					= command_frameskip,
	["pause"]						= command_pause,
	["resume"]						= command_resume,
	["input"]						= command_input,
	["paste"]						= command_paste,
	["set_attenuation"]				= command_set_attenuation,
	["set_natural_keyboard_in_use"]	= command_set_natural_keyboard_in_use,
	["state_load"]					= command_state_load,
	["state_save"]					= command_state_save,
	["save_snapshot"]				= command_save_snapshot,
	["load"]						= command_load,
	["unload"]						= command_unload,
	["create"]						= command_create,
	["seq_poll_start"]				= command_seq_poll_start,
	["seq_poll_stop"]				= command_seq_poll_stop,
	["set_input_value"]				= command_set_input_value,
	["show_profiler"]				= command_show_profiler
}

-- invokes a command line
function invoke_command_line(line)
	-- invoke the appropriate command
	local invocation = (function()
		local args = quoted_string_split(line)
		if (commands[args[1]:lower()]) then
			commands[args[1]:lower()](args)
		else
			command_unknown(args)
		end
	end)

	local status, err = pcall(invocation)
	if (not status) then
		print("ERROR ## Lua runtime error " .. tostring(err.code))
	end
end

function console.startplugin()
	-- start a thread to read from stdin
	local scr = "return io.read()"
	local conth = emu.thread()
	conth:start(scr);

	-- we want to hold off until the prestart event; register a handler for it
	local initial_start = false
	local session_active = true
	emu.register_prestart(function()
		-- prestart has been invoked; set up MAME for our control
		emu.pause()
		manager:machine():uiinput().presses_enabled = false
		session_active = true

		-- is this the very first time we have hit a pre-start?
		if not initial_start then
			-- and indicate that we're ready for commands
			print("OK STATUS ### Emulation commenced; ready for commands")
			emit_status()
			initial_start = true
		end
	end)

	emu.register_stop(function()
		-- the emulation session has stopped; tidy things up
		stop_polling_input_seq()
		session_active = false
	end)

	-- register another handler to handle commands after prestart
	emu.register_periodic(function()
		-- it is essential that we only perform these activities when there
		-- is an active session!
		if session_active then
			-- are we polling input?
			if is_polling_input_seq() then
				if manager:machine():input():seq_poll() then
					-- done polling; set the new input_seq
					local seq = manager:machine():input():seq_poll_final()
					manager:machine():input():seq_pressed(seq)
					current_poll_field:set_input_seq(current_poll_seq_type, seq)
					
					-- and terminate polling
					stop_polling_input_seq()
				end
			end

			-- do we have a command?
			if not (conth.yield or conth.busy) then
				-- invoke the command line
				invoke_command_line(conth.result)

				-- continue on reading
				conth:start(scr)
			end
		end
	end)
end

return exports
