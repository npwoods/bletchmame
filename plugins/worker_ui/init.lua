local exports = {}
exports.name = "workerui"
exports.version = "0.0.1"
exports.description = "worker-ui plugin"
exports.license = "The BSD 3-Clause License"
exports.author = { name = "Bletch" }

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

function split(text, delim)
    -- returns an array of fields based on text and delimiter (one character only)
    local result = {}
    local magic = "().%+-*?[]^$"

    if delim == nil then
        delim = "%s"
    elseif string.find(delim, magic, 1, true) then
        -- escape magic
        delim = "%"..delim
    end

    local pattern = "[^"..delim.."]+"
    for w in string.gmatch(text, pattern) do
        table.insert(result, w)
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

function utf8_process(str, callback)
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
			callback(val)
			val = 0
		end
	end
end

function xml_encode(str)
	local res = ""
	local callback = (function(val)
		if (val > 127 or val == 9 or val == 10 or val == 13) then
			res = res .. "&#" .. tostring(val) .. ";"			-- control and non-ASCII characters
		elseif (val < 32) then
			res = res .. "&#" .. tostring(val + 0xE000) .. ";"	-- control characters illegal in XML 1.0
		elseif string.char(val) == "\"" then
			res = res .. "&quot;"								-- quotation mark
		elseif string.char(val) == "&" then
			res = res .. "&amp;"								-- ampersand
		elseif string.char(val) == "<" then
			res = res .. "&lt;"									-- less than sign
		elseif string.char(val) == ">" then
			res = res .. "&gt;"									-- greater than sign
		else
			res = res .. string.char(val)						-- characters legal in XML 1.0
		end
	end)
	utf8_process(str, callback)
	return res
end

-- before MAME 0.227, the tag was a method (e.g. - device:tag()) but starting with
-- MAME 0.227, it is now a property (e.g. - device.tag); we need to support both
function get_device_tag_init(device)
	if device.tag and type(device.tag) == "function" then
		get_device_tag = function(dev) return dev:tag() end
	else
		get_device_tag = function(dev) return dev.tag end
	end
	return get_device_tag(device)
end
local get_device_tag = get_device_tag_init

-- get the running_machine - this is different in MAME 0.227 and beyond
local machine, machine_debugger, machine_input, machine_ioport, machine_natkeyboard
local machine_options, machine_sound, machine_uiinput, machine_video, ui, plugins
if (type(manager.machine) == "function") then
	machine				= function() return manager:machine() end
	machine_debugger	= function() return manager:machine():debugger() end
	machine_input		= function() return manager:machine():input() end
	machine_ioport		= function() return manager:machine():ioport() end
	machine_natkeyboard	= function() return manager:machine():ioport():natkeyboard() end
	machine_options		= function() return manager:machine():options() end
	machine_sound		= function() return manager:machine():sound() end
	machine_uiinput		= function() return manager:machine():uiinput() end
	machine_video		= function() return manager:machine():video() end
	ui					= function() return manager:ui() end
	plugins				= function() return manager:plugins() end
else
	machine				= function() return manager.machine end
	machine_debugger	= function() return manager.machine.debugger end
	machine_input		= function() return manager.machine.input end
	machine_ioport		= function() return manager.machine.ioport end
	machine_natkeyboard	= function() return manager.machine.natkeyboard end
	machine_options		= function() return manager.machine.options end
	machine_sound		= function() return manager.machine.sound end
	machine_uiinput		= function() return manager.machine.uiinput end
	machine_video		= function() return manager.machine.video end
	ui					= function() return manager.ui end
	plugins				= function() return manager.plugins end
end

function get_images()
	-- return a list of images, avoiding dupes
	--
	-- Before MAME 0.227, the images collection could be indexed by both instance_name and
	-- brief_instance_name, but starting with MAME 0.227, it is indexed by tag.  This
	-- technique of building a table is neutral to these changes
	local result = {}
	for _,image in pairs(machine().images) do
		result[get_device_tag(image.device)] = image
	end
	return result
end

function find_image_by_tag(tag)
	if not (tag.sub(1, 1) == ":") then
		tag = ":" .. tag
	end

	for _,image in pairs(get_images()) do
		if get_device_tag(image.device) == tag then
			return image
		end
	end
end

function find_port_and_field(tag, mask)
	if not (tag.sub(1, 1) == ":") then
		tag = ":" .. tag
	end

	local port = machine_ioport().ports[tag]
	if not port then
		return
	end

	for k,v in pairs(port.fields) do
		if v.mask == tonumber(mask) then
			return v
		end
	end
end

-- global state
local current_poll_callback = nil
local mouse_enabled_by_ui = false
local pause_when_restarted = true

function is_polling_input_seq()
	if current_poll_callback then
		return true
	else
		return false
	end
end

function update_mouse_enabled()
	local enabled = mouse_enabled_by_ui and not is_polling_input_seq()
	pcall(function() machine_input().device_classes["mouse"].enabled = enabled end)
end

function stop_polling_input_seq()
	current_poll_callback = nil
	ui():set_aggressive_input_focus(false)
	update_mouse_enabled()
end

function has_input_using_mouse()
	local result = false

	function check_seq(field, seq_type)
		-- check this input seq for mouse codes; we clean the seq before checking because if
		-- it references an unknown mouse we don't care about it
		local seq = field:input_seq(seq_type)
		local cleaned_seq = machine_input():seq_clean(seq)
		local tokens = machine_input():seq_to_tokens(cleaned_seq)

		for _, token in pairs(split(tokens)) do
			if (string.match(tokens, "MOUSECODE_")) then
				result = true
			end
		end
	end

	for _,port in pairs(machine_ioport().ports) do
		for _,field in pairs(port.fields) do
			check_seq(field, "standard")
			if field.is_analog then
				check_seq(field, "decrement")
				check_seq(field, "increment")
			end
		end
	end

	return result
end

function get_slot_option(tag)
	if (tag:sub(1, 1) == ":") then
		tag = tag:sub(2, tag:len())
	end
	return machine_options():slot_option(tag)
end

function emit_status(light, out)
	if light == nil then
		light = false
	end
	local emit
	local opened_file
	if out then
		-- we've been called with an output, likely in a debugging scenario; this
		-- could either be a file or a file name (string)
		if type(out) == "string" then
			-- normalize as a file
			opened_file = assert(io.open(out, "w"))
			out = opened_file
		end
		emit = (function(s)
			out:write(s)
			out:write("\n")
		end)
	else
		emit = print
	end

	-- abstractions to hide some differences between MAME 0.227 and
	-- previous versions, similar to get_device_tag
	local get_item_code, get_image_filename
	local get_speed_percent, get_effective_frameskip, get_is_recording
	if type(machine_ioport().natkeyboard) == "function" then
		get_item_code			= function(item) return item:code() end
		get_image_filename		= function(image) return image:filename() end
		get_speed_percent		= function() return machine_video():speed_percent() end
		get_effective_frameskip	= function() return machine_video():effective_frameskip() end
		get_is_recording		= function() return machine_video():is_recording() end
	else
		get_item_code			= function(item) return item.code end
		get_image_filename		= function(image) return image.filename end
		get_speed_percent		= function() return machine_video().speed_percent end
		get_effective_frameskip	= function() return machine_video().effective_frameskip end
		get_is_recording		= function() return machine_video().is_recording end
	end

	emit("<status");
	emit("\tphase=\"running\"");
	emit("\tpolling_input_seq=\"" .. tostring(is_polling_input_seq()) .. "\"");
	emit("\tnatural_keyboard_in_use=\"" .. tostring(machine_natkeyboard().in_use) .. "\"");
	emit("\tpaused=\"" .. tostring(machine().paused) .. "\"");
	emit("\tstartup_text=\"\"");
	emit("\tdebugger_present=\"" .. string_from_bool(machine_debugger()) .. "\"");
	emit("\tshow_profiler=\"" .. tostring(ui().show_profiler) .. "\"");
	if (not light) then
		emit("\thas_input_using_mouse=\"" .. tostring(has_input_using_mouse()) .. "\"");
	end
	emit(">");

	-- <video> (video_manager)
	emit("\t<video");
	emit("\t\tspeed_percent=\"" .. tostring(get_speed_percent()) .. "\"");
	emit("\t\tframeskip=\"" .. tostring(machine_video().frameskip) .. "\"");
	emit("\t\teffective_frameskip=\"" .. tostring(get_effective_frameskip()) .. "\"");
	emit("\t\tthrottled=\"" .. tostring(machine_video().throttled) .. "\"");
	emit("\t\tthrottle_rate=\"" .. tostring(machine_video().throttle_rate) .. "\"");
	emit("\t\tis_recording=\"" .. string_from_bool(get_is_recording()) .. "\"");
	emit("\t/>");

	-- <sound> (sound_manager)
	emit("\t<sound");
	emit("\t\tattenuation=\"" .. tostring(machine_sound().attenuation) .. "\"");
	emit("\t/>");

	-- <cheats> (cheat manager)
	if (_G and _G.emu and  _G.emu.plugin and _G.emu.plugin.cheat) then
		emit("\t<cheats>");
		for id,desc in pairs(_G.emu.plugin.cheat:list()) do
			local cheat = _G.emu.plugin.cheat.get(id)
			emit(string.format("\t\t<cheat id=\"%s\" enabled=\"%s\" description=\"%s\"",
				tostring(id),
				string_from_bool(cheat.enabled),
				xml_encode(desc)))
			if (cheat.script) then
				emit(string.format("\t\t\thas_run_script=\"%s\" has_on_script=\"%s\" has_off_script=\"%s\" has_change_script=\"%s\"",
					string_from_bool(cheat.script.run),
					string_from_bool(cheat.script.on),
					string_from_bool(cheat.script.off),
					string_from_bool(cheat.script.change)))
			end
			if (cheat.comment) then
				emit(string.format("\t\t\tcomment=\"%s\"", xml_encode(cheat.comment)))
			end
			emit("\t\t\t>")
			if cheat.parameter then
				emit(string.format("\t\t\t<parameter value=\"%s\" minimum=\"%s\" maximum=\"%s\" step=\"%s\">",
					cheat.parameter.value,
					cheat.parameter.min,
					cheat.parameter.max,
					cheat.parameter.step))
				if cheat.parameter.item then
					for item_value,item_obj in pairs(cheat.parameter.item) do
						emit(string.format("\t\t\t\t<item value=\"%s\" text=\"%s\"/>",
							tostring(item_value),
							xml_encode(item_obj.text)))
					end
				end
				emit("\t\t\t</parameter>")
			end
			emit("\t\t</cheat>")
		end
		emit("\t</cheats>");
	end

	if (not light or machine().paused or is_polling_input_seq()) then
		-- <images>
		emit("\t<images>")
		for _,image in pairs(get_images()) do
			local filename = get_image_filename(image)
			if filename == nil then
				filename = ""
			end

			-- basic image properties
			emit(string.format("\t\t<image tag=\"%s\" instance_name=\"%s\" is_readable=\"%s\" is_writeable=\"%s\" is_creatable=\"%s\" must_be_loaded=\"%s\"",
				xml_encode(get_device_tag(image.device)),
				xml_encode(image.instance_name),
				string_from_bool(image.is_readable),
				string_from_bool(image.is_writeable),
				string_from_bool(image.is_creatable),
				string_from_bool(image.must_be_loaded)))

			-- filename
			if filename ~= nil and filename ~= "" then
				emit("\t\t\tfilename=\"" .. xml_encode(filename) .. "\"")
			end

			-- display
			local display = image:display()
			if display ~= nil and display ~= "" then
				emit("\t\t\tdisplay=\"" .. xml_encode(display) .. "\"")
			end
			emit("\t\t>")

			-- formats
			if pcall(function() return image.formatlist end) and image.formatlist ~= nil then
				emit("\t\t\t<formats>")
				for _,format in pairs(image.formatlist) do
					emit(string.format("\t\t\t\t<format name=\"%s\" description=\"%s\" option_spec=\"%s\">",
						xml_encode(format.name),
						xml_encode(format.description),
						xml_encode(format.option_spec)))
					for _,ext in pairs(format.extensions) do
						emit(string.format("\t\t\t\t\t<extension>%s</extension>", xml_encode(ext)))
					end
					emit("\t\t\t\t</format>")
				end
				emit("\t\t\t</formats>")
			end

			emit("\t\t</image>")
		end	
		emit("\t</images>")

		-- <slots>
		if pcall(function() return machine().slots end) then
			emit("\t<slots>");
			for name,slot in pairs(machine().slots) do
				-- perform logic equivalent to menu_slot_devices::get_current_option()
				local current_option_name
				if (slot.fixed) then
					current_option_name = slot:default_option()
				else
					local current_option = get_slot_option(name)
					if current_option then
						current_option_name = current_option.value
					end
				end

				emit(string.format("\t\t<slot name=\"%s\" fixed=\"%s\" has_selectable_options=\"%s\"",
					xml_encode(name),
					string_from_bool(slot.fixed),
					string_from_bool(slot.has_selectable_options)))
				if (current_option_name) then
					emit(string.format("\t\t\tcurrent_option=\"%s\"", xml_encode(current_option_name)))
				end
				emit("\t\t>")				
				for optname,option in pairs(slot.options) do
					emit(string.format("\t\t\t<option name=\"%s\" selectable=\"%s\"/>",
						xml_encode(optname),
						string_from_bool(option.selectable)))
				end
				emit("\t\t</slot>")
			end
			emit("\t</slots>");
		end

		-- <inputs>
		emit("\t<inputs>")
		for _,port in pairs(machine_ioport().ports) do
			for _,field in pairs(port.fields) do
				if field.enabled then
					local type_class = field.type_class
					local is_switch = type_class == "dipswitch" or type_class == "config"

					emit("\t\t<input"
						.. " port_tag=\"" .. xml_encode(get_device_tag(port)) .. "\""
						.. " mask=\"" .. tostring(field.mask) .. "\""
						.. " class=\"" .. type_class .. "\""
						.. " group=\"" .. tostring(machine_ioport():type_group(field.type, field.player)) .. "\""
						.. " type=\"" .. field.type .. "\""
						.. " player=\"" .. field.player .. "\""
						.. " is_analog=\"" .. string_from_bool(field.is_analog) .. "\""
						.. " name=\"" .. xml_encode(field.name) .. "\"")

					local first_keyboard_code = field:keyboard_codes(0)[1]
					if first_keyboard_code then
						local val
						local callback = (function(x)
							if (val == nil) then
								val = x
							end
						end)
						utf8_process(first_keyboard_code, callback)

						emit("\t\t\tfirst_keyboard_code=\"" .. tostring(val) .. "\"")
					end

					if is_switch then
						-- DIP switches and configs have values
						emit("\t\t\tvalue=\"" .. tostring(field.user_value) .. "\"")
					end

					emit("\t\t>")

					-- emit input sequences for anything that is not DIP switches of configs
					if not is_switch then
						if field.is_analog then
							-- analog sequences have increment/decrement in addition to "standard"
							seq_types = {"standard", "increment", "decrement"}
						else
							-- digital sequences just have increment
							seq_types = {"standard"}
						end

						for _,seq_type in pairs(seq_types) do
							emit("\t\t\t<seq type=\"" .. seq_type
								.. "\" tokens=\"" .. xml_encode(machine_input():seq_to_tokens(field:input_seq(seq_type)))
								.. "\"/>")
						end
					end

					emit("\t\t</input>")
				end
			end
		end
		emit("\t</inputs>")

		emit("\t<input_devices>")
		for _,devclass in pairs(machine_input().device_classes) do
			emit("\t\t<class name=\"" .. xml_encode(devclass.name)
				.. "\" enabled=\"" .. string_from_bool(devclass.enabled)
				.. "\" multi=\"" .. string_from_bool(devclass.multi) .. "\">")
			for _,device in pairs(devclass.devices) do
				emit("\t\t\t<device name=\"" .. xml_encode(device.name)
					.. "\" id=\"" .. xml_encode(device.id)
					.. "\" devindex=\"" .. device.devindex .. "\">")
				for id,item in pairs(device.items) do
					emit("\t\t\t\t<item name=\"" .. xml_encode(item.name)
						.. "\" token=\"" .. xml_encode(item.token)
						.. "\" code=\"" .. xml_encode(machine_input():code_to_token(get_item_code(item)))
						.. "\"/>")
				end
				emit("\t\t\t</device>")
			end
			emit("\t\t</class>")
		end
		emit("\t</input_devices>")
	end

	emit("</status>");

	if opened_file then
		opened_file:close()
	end
end

-- EXIT command
function command_exit(args)
	machine():exit()
	print("@OK ### Exit scheduled")
end

-- PING command
local next_ping_should_be_light = false
function command_ping(args)
	print("@OK STATUS ### Ping... pong...")
	emit_status(next_ping_should_be_light)
	next_ping_should_be_light = true
end

-- SOFT_RESET command
function command_soft_reset(args)
	machine():soft_reset()
	print("@OK ### Soft Reset Scheduled")
end

-- HARD_RESET command
function command_hard_reset(args)
	machine():hard_reset()
	print("@OK ### Hard Reset Scheduled")
end

-- PAUSE command
function command_pause(args)
	emu.pause()
	print("@OK STATUS ### Paused")
	emit_status()
end

-- RESUME command
function command_resume(args)
	emu.unpause()
	print("@OK STATUS ### Resumed")
	emit_status()
end

-- DEBUGGER command
function command_debugger(args)
	if not machine_debugger() then
		print("@ERROR ### Debugger not present")
		return
	end

	machine_debugger().execution_state = 'stop'
	print("@OK ### Dropping into debugger")
end

-- THROTTLED command
function command_throttled(args)
	machine_video().throttled = toboolean(args[2])
	print("@OK STATUS ### Throttled set to " .. tostring(machine_video().throttled))
	emit_status()
end

-- THROTTLE_RATE command
function command_throttle_rate(args)
	machine_video().throttle_rate = tonumber(args[2])
	print("@OK STATUS ### Throttle rate set to " .. tostring(machine_video().throttle_rate))
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
	machine_video().frameskip = frameskip
	print("@OK STATUS ### Frame skip rate set to " .. tostring(machine_video().frameskip))
	emit_status()
end

-- INPUT command
function command_input(args)
	machine_ioport():natkeyboard():post(args[2])
	print("@OK ### Text inputted")
end

-- PASTE command
function command_paste(args)
	machine_ioport():natkeyboard():paste(args[2])
	print("@OK ### Text inputted from clipboard")
end

-- SET_ATTENUATION command
function command_set_attenuation(args)
	machine_sound().attenuation = tonumber(args[2])
	print("@OK STATUS ### Sound attenuation set to " .. tostring(machine_sound().attenuation))
	emit_status()
end

-- SET_NATURAL_KEYBOARD_IN_USE command
function command_set_natural_keyboard_in_use(args)
	machine_ioport():natkeyboard().in_use = toboolean(args[2])
	print("@OK STATUS ### Natural keyboard in use set to " .. tostring(machine_ioport():natkeyboard().in_use))
	emit_status()
end

-- STATE_LOAD command
function command_state_load(args)
	machine():load(args[2])
	print("@OK ### Scheduled state load of '" .. args[2] .. "'")
end

-- STATE_SAVE command
function command_state_save(args)
	machine():save(args[2])
	print("@OK ### Scheduled state save of '" .. args[2] .. "'")
end

-- STATE_SAVE_AND_EXIT command
function command_state_save_and_exit(args)
	machine():save(args[2])
	machine():exit()
	print("@OK ### Scheduled state save of '" .. args[2] .. "' and an exit")
end

-- SAVE_SNAPSHOT command
function command_save_snapshot(args)
	-- hardcoded for first screen now
	local index = tonumber(args[2])
	for k,v in pairs(machine().screens) do
		if index == 0 then
			screen = v
			break
		end
		index = index - 1
	end

	screen:snapshot(args[3])
	print("@OK ### Successfully saved screenshot '" .. args[3] .. "'")
end

-- BEGIN_RECORDING command
function command_begin_recording(args)
	machine_video():begin_recording(args[2], args[3])
	print("@OK STATUS ### Began recording '" .. args[2] .. "'")
	emit_status()
end

-- END_RECORDING command
function command_end_recording(args)
	machine_video():end_recording()
	print("@OK STATUS ### Ended recording")
	emit_status()
end

-- LOAD command
function command_load(args)
	-- loop; this is a batch command
	for i = 2,#args-1,2 do		
		local image = find_image_by_tag(args[i+0])
		if not image then
			print("@ERROR ### Cannot find device '" .. args[i+0] .. "'")
			return
		end
		image:load(args[i+1])
	end
	print("@OK STATUS ### Device '" .. args[2] .. "' loaded '" .. args[3] .. "' successfully")
	emit_status()
end

-- UNLOAD command
function command_unload(args)
	local image = find_image_by_tag(args[2])
	if not image then
		print("@ERROR ### Cannot find device '" .. args[2] .. "'")
		return
	end
	image:unload()
	print("@OK STATUS ### Device '" .. args[2] .. "' unloaded successfully")
	emit_status()
end

-- CREATE command
function command_create(args)
	local image = find_image_by_tag(args[2])
	if not image then
		print("@ERROR ### Cannot find device '" .. args[2] .. "'")
		return
	end
	image:create(args[3])
	print("@OK STATUS ### Device '" .. args[2] .. "' created image '" .. args[3] .. "' successfully")
	emit_status()
end

-- CHANGE_SLOTS Command
function command_change_slots(args)
	for i = 2,#args-1,2 do
		local slot_option_name = args[i + 0]
		local slot_option_value = args[i + 1]
		local opt = get_slot_option(slot_option_name)
		if not opt then
			print("@ERROR ### Cannot find slot option '" .. slot_option_name .. "'")
			return
		end
		opt:specify(slot_option_value)
	end
	pause_when_restarted = machine().paused
	machine():hard_reset()
	print("@OK ### Slots changed and hard reset scheduled")
end

-- SEQ_SET command
function command_seq_set(args)
	local field_ids = ""

	-- loop; this is a batch command
	for i = 2,#args-3,4 do
		-- get fields off of args
		local port_tag = args[i + 0]
		local mask = tonumber(args[i + 1])
		local seq_type = args[i + 2]
		local tokens = args[i + 3]
		local field_id = port_tag .. ":" .. tostring(mask)

		-- identify port and field
		local field = find_port_and_field(port_tag, mask)
		if not field then
			print("@ERROR ### Can't find field mask '" .. tostring(mask) .. "' on port '" .. port_tag .. "'")
			return
		end
		if not field.enabled then
			print("@ERROR ### Field '" .. field_id .. "' is disabled")
			return
		end

		-- set the input seq with the specified tokens (or "*" for default)
		local seq
		if (tokens == "*") then
			seq = field:default_input_seq(seq_type)
		else
			seq = machine_input():seq_from_tokens(tokens)
		end
		field:set_input_seq(seq_type, seq)

		-- append the ids
		if (field_ids ~= "") then
			field_ids = field_ids .. "," .. field_id
		else
			field_ids = field_id
		end
	end

	-- and report success
	print("@OK STATUS ### Input seqs set: " .. field_ids)
	emit_status()
end

-- SEQ_POLL_START command
function command_seq_poll_start(args)
	-- identify port and field
	local field = find_port_and_field(args[2], args[3])
	if not field then
		print("@ERROR ### Can't find field mask '" .. tostring(tonumber(args[3])) .. "' on port '" .. args[2] .. "'")
		return
	end
	if not field.enabled then
		print("@ERROR ### Field '" .. args[2] .. "':" .. tostring(tonumber(args[3])) .. " is disabled")
		return
	end

	-- optional start seq
	local start_seq
	if (args[5] and args[5] ~= "") then
		start_seq = machine_input():seq_from_tokens(args[5])
	end

	-- start polling! (this was changed radically in MAME 0.227, so this is quite complicated and it
	-- also involves setting up seq_poll_continue to hide these nuances)
	local seq_poll_continue
	if machine_input().seq_poll_start ~= nil then
		-- MAME 0.226 and prior technique
		local input_seq_class
		if (field.is_analog and args[4] == "standard") then
			input_seq_class = "absolute"
		else
			input_seq_class = "switch"
		end

		-- start polling
		machine_input():seq_poll_start(input_seq_class, start_seq)

		-- and prepare seq_poll_continue
		seq_poll_continue = function()
			if machine_input():seq_poll() then
				return machine_input():seq_poll_final()
			end
		end
	else
		-- MAME 0.227 and later technique
		local sequence_poller
		if (field.is_analog and args[4] == "standard") then
			sequence_poller = machine_input():axis_sequence_poller()
		else
			sequence_poller = machine_input():switch_sequence_poller()
		end

		-- start polling
		if (start_seq) then
			sequence_poller:start(start_seq)
		else
			sequence_poller:start()
		end

		-- and prepare seq_poll_continue
		seq_poll_continue = function()
			if sequence_poller:poll() then
				return sequence_poller.sequence
			end
		end
	end

	-- set up the callback and tidy things up
	current_poll_callback = function()
		local final_seq = seq_poll_continue()
		if final_seq then
			-- we got something - specify the input seq
			machine_input():seq_pressed(final_seq)
			field:set_input_seq(args[4], final_seq)
					
			-- and terminate polling
			stop_polling_input_seq()
		end
	end
	ui():set_aggressive_input_focus(true)
	update_mouse_enabled()
	print("@OK STATUS ### Starting polling")
	emit_status()
end

-- SEQ_POLL_STOP command
function command_seq_poll_stop(args)
	stop_polling_input_seq()
	print("@OK STATUS ### Stopped polling");
	emit_status()
end

-- SET_INPUT_VALUE command
function command_set_input_value(args)
	local field = find_port_and_field(args[2], args[3])
	if not field then
		print("@ERROR ### Can't find field mask '" .. tostring(tonumber(args[3])) .. "' on port '" .. args[2] .. "'")
		return
	end
	if not field.enabled then
		print("@ERROR ### Field '" .. args[2] .. "':" .. tostring(tonumber(args[3])) .. " is disabled")
		return
	end

	field.user_value = tonumber(args[4]);
	print("@OK STATUS ### Field '" .. args[2] .. "':" .. tostring(args[3]) .. " set to " .. tostring(field.user_value))
	emit_status()
end

-- SET_MOUSE_ENABLED command
function command_set_mouse_enabled(args)
	mouse_enabled_by_ui = toboolean(args[2])
	update_mouse_enabled()
	print("@OK ### Mouse enabled set to " .. tostring(toboolean(args[2])))
end

-- SHOW_PROFILER command
function command_show_profiler(args)
	ui().show_profiler = toboolean(args[2])
	print("@OK STATUS ### Show profiler set to " .. tostring(ui().show_profiler))
	emit_status()
end

-- SET_CHEAT_STATE command
function command_set_cheat_state(args)
	local cheat_id = tonumber(args[2])
	local cheat = _G.emu.plugin.cheat.get(cheat_id)
	if not cheat then
		print("@ERROR ### Can't find cheat '" .. tostring(cheat_id) .. "'")
		return
	end

	-- set the enabled value
	local enabled = toboolean(args[3])
	cheat:set_enabled(enabled)

	-- set the parameter value
	if args[4] then
		local parameter = tonumber(args[4])
		cheat:set_value(parameter)
	end

	print("@OK STATUS ### Set cheat '" .. tostring(cheat_id) .. "' enabled to '" .. tostring(enabled) .. "'")
	emit_status()
end

-- DUMP_STATUS command
function command_dump_status(args)
	local filename = args[2]
	local out = io.open(filename, "w")
	emit_status(false, out)
	io.close(out)
	print("@OK ### Status dumped to \"" .. filename .. "\"")
end

-- arbitrary Lua
function command_lua(expr)
	local func, err = load(expr)
	if not func then
		print("@ERROR ### " .. tostring(err))
		return
	end
	local result = func()
	if (result == nil) then
		print("@OK ### Command evaluated")
	else
		print("@OK ### Command evaluated; result = " .. tostring(result))
	end
end

-- not implemented command
function command_nyi(args)
	print("@ERROR ### Command '" .. args[1] .. "' not yet implemeted")
end

-- unknown command
function command_unknown(args)
	print("@ERROR ### Unrecognized command '" .. args[1] .. "'")
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
	["debugger"]					= command_debugger,
	["input"]						= command_input,
	["paste"]						= command_paste,
	["set_attenuation"]				= command_set_attenuation,
	["set_natural_keyboard_in_use"]	= command_set_natural_keyboard_in_use,
	["state_load"]					= command_state_load,
	["state_save"]					= command_state_save,
	["state_save_and_exit"]			= command_state_save_and_exit,
	["save_snapshot"]				= command_save_snapshot,
	["begin_recording"]				= command_begin_recording,
	["end_recording"]				= command_end_recording,
	["load"]						= command_load,
	["unload"]						= command_unload,
	["create"]						= command_create,
	["change_slots"]				= command_change_slots,
	["seq_set"]						= command_seq_set,
	["seq_poll_start"]				= command_seq_poll_start,
	["seq_poll_stop"]				= command_seq_poll_stop,
	["set_input_value"]				= command_set_input_value,
	["set_mouse_enabled"]			= command_set_mouse_enabled,
	["show_profiler"]				= command_show_profiler,
	["set_cheat_state"]				= command_set_cheat_state,
	["dump_status"]					= command_dump_status
}

-- invokes a command line
function invoke_command_line(line)
	-- invoke the appropriate command
	local invocation = (function()
		-- check for "?" syntax
		if (line:sub(1, 1) == "?") then
			command_lua(line:sub(2))
		else
			-- split the arguments and invoke
			local args = quoted_string_split(line)
			if (commands[args[1]:lower()]) then
				commands[args[1]:lower()](args)
			else
				command_unknown(args)
			end
		end
	end)

	local status, err = pcall(invocation)
	if (not status) then
		print("@ERROR ## Lua runtime error " .. tostring(err))
	end
end

function startplugin()
	-- start a thread to read from stdin
	local scr = "return io.read()"
	local conth = emu.thread()
	conth:start(scr);

	-- we want to hold off until the prestart event; register a handler for it
	local initial_start = false
	local session_active = true
	emu.register_prestart(function()
		-- prestart has been invoked; set up MAME for our control
		machine_uiinput().presses_enabled = false
		if machine_debugger() then
			machine_debugger().execution_state = 'run'
		end
		session_active = true
		update_mouse_enabled()

		-- do we have to pause now that we [re]started?
		if pause_when_restarted then
			emu.pause()
			pause_when_restarted = true
		end

		-- is this the very first time we have hit a pre-start?
		if not initial_start then
			-- and indicate that we're ready for commands
			print("@OK STATUS ### Emulation commenced; ready for commands")
			emit_status()
			initial_start = true
		end

		-- since we had a reset, we might have images that were just loaded, therefore
		-- the status returned by the next PING should not be light
		 next_ping_should_be_light = false
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
			-- are we polling input?  if so, poll!
			if is_polling_input_seq() then
				current_poll_callback()
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

	-- we do not want to use MAME's internal file manager - register a function
	emu.register_mandatory_file_manager_override(function(instance_name)
		return true
	end)

	-- we want to override MAME's default settings; by default MAME associates most joysticks
	-- with the mouse and vice versa.  This is because normal MAME usage from the command line
	-- will turn on input classes with options like '-[no]mouse'.  We want to automatically turn
	-- these on, but we need to apply a treatment to defaults that assume normal MAME
	emu.register_before_load_settings(function()
		function fix_default_input_seq(field, seq_type)
			local old_default_seq = field:default_input_seq(seq_type)
			local cleaned_default_seq = machine_input():seq_clean(old_default_seq)
			local cleaned_default_seq_tokens = machine_input():seq_to_tokens(cleaned_default_seq)
			local match = string.match(cleaned_default_seq_tokens, "JOYCODE_[0-9A-Z_]+ OR MOUSECODE_[0-9A-Z_]+")
			if match then		
				local new_default_seq_tokens = string.match(cleaned_default_seq_tokens, "JOYCODE_[0-9A-Z_]+")
				if new_default_seq_tokens then
					local new_default_seq = machine_input():seq_from_tokens(new_default_seq_tokens)
					field:set_default_input_seq(seq_type, new_default_seq)
				end
			end
		end

		for _,port in pairs(machine_ioport().ports) do
			for _,field in pairs(port.fields) do
				fix_default_input_seq(field, "standard")
				if field.is_analog then
					fix_default_input_seq(field, "decrement")
					fix_default_input_seq(field, "increment")
				end
			end
		end
	end)

	-- activate the cheat plugin, if present (and not started separately)
	local cheatentry = plugins()["cheat"]
	local cheatplugin
	if cheatentry and not cheatentry.start then -- don't try to start it twice
		local stat, res = pcall(require, cheatentry.name)
		if stat then
			cheatplugin = res
			cheatplugin.startplugin()
			if cheatplugin.set_folder~=nil then cheatplugin.set_folder(cheatentry.directory) end
		end
	end
end

function exports.startplugin()
	-- run startplugin through pcall; we want to catch errors
	local status, err = pcall(startplugin)
	if (not status) then
		print("@ERROR ## Lua runtime error on plugin startup " .. tostring(err))
	end
end

return exports
