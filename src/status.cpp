/***************************************************************************

    status.cpp

	Tracking of MAME internal state updates

***************************************************************************/

#include <QTextStream>

#include "status.h"
#include "xmlparser.h"


//**************************************************************************
//  VARIABLES
//**************************************************************************

static const util::enum_parser<status::machine_phase> s_machine_phase_parser =
{
	{ "preinit", status::machine_phase::PREINIT },
	{ "init", status::machine_phase::INIT },
	{ "reset", status::machine_phase::RESET },
	{ "running", status::machine_phase::RUNNING },
	{ "exit", status::machine_phase::EXIT }
};

static const util::enum_parser<status::input::input_class> s_input_class_parser =
{
	{ "controller", status::input::input_class::CONTROLLER, },
	{ "misc", status::input::input_class::MISC, },
	{ "keyboard", status::input::input_class::KEYBOARD, },
	{ "config", status::input::input_class::CONFIG, },
	{ "dipswitch", status::input::input_class::DIPSWITCH, },
};

static const util::enum_parser<status::input_seq::type> s_inputseq_type_parser =
{
	{ "standard", status::input_seq::type::STANDARD },
	{ "increment", status::input_seq::type::INCREMENT },
	{ "decrement", status::input_seq::type::DECREMENT }
};


//**************************************************************************
//  MISCELLANEOUS STATUS OBJECTS
//**************************************************************************

//-------------------------------------------------
//  image::operator==
//-------------------------------------------------

bool status::image::operator==(const status::image &that) const
{
	return m_tag == that.m_tag
		&& m_instance_name == that.m_instance_name
		&& m_is_readable == that.m_is_readable
		&& m_is_writeable == that.m_is_writeable
		&& m_is_creatable == that.m_is_creatable
		&& m_must_be_loaded == that.m_must_be_loaded
		&& m_file_name == that.m_file_name
		&& m_display == that.m_display;
}


//-------------------------------------------------
//  slot::operator==
//-------------------------------------------------

bool status::slot::operator==(const status::slot &that) const
{
	return m_name == that.m_name
		&& m_current_option == that.m_current_option
		&& m_fixed == that.m_fixed;
}


//-------------------------------------------------
//  input_seq::operator==
//-------------------------------------------------

bool status::input_seq::operator==(const status::input_seq &that) const
{
	return m_type == that.m_type
		&& m_tokens == that.m_tokens;
}


//-------------------------------------------------
//  input::operator==
//-------------------------------------------------

bool status::input::operator==(const status::input &that) const
{
	return m_port_tag == that.m_port_tag
		&& m_name == that.m_name
		&& m_mask == that.m_mask
		&& m_class == that.m_class
		&& m_group == that.m_group
		&& m_player == that.m_player
		&& m_type == that.m_type
		&& m_is_analog == that.m_is_analog
		&& m_first_keyboard_code == that.m_first_keyboard_code
		&& m_value == that.m_value
		&& m_seqs == that.m_seqs;
}


//-------------------------------------------------
//  input_device_item::operator==
//-------------------------------------------------

bool status::input_device_item::operator==(const status::input_device_item &that) const
{
	return m_name == that.m_name
		&& m_token == that.m_token
		&& m_code == that.m_code;
}


//-------------------------------------------------
//  input_device::operator==
//-------------------------------------------------

bool status::input_device::operator==(const status::input_device &that) const
{
	return m_name == that.m_name
		&& m_id == that.m_id
		&& m_items == that.m_items;
}


//-------------------------------------------------
//  input_class::operator==
//-------------------------------------------------

bool status::input_class::operator==(const status::input_class &that) const
{
	return m_name == that.m_name
		&& m_enabled == that.m_enabled
		&& m_multi == that.m_multi
		&& m_devices == that.m_devices;
}


//-------------------------------------------------
//  cheat_parameter::operator==
//-------------------------------------------------

bool status::cheat_parameter::operator==(const status::cheat_parameter &that) const
{
	return m_value == that.m_value
		&& m_minimum == that.m_minimum
		&& m_maximum == that.m_maximum
		&& m_step == that.m_step
		&& m_items == that.m_items;
}


//-------------------------------------------------
//  cheat::operator==
//-------------------------------------------------

bool status::cheat::operator==(const status::cheat &that) const
{
	return m_id == that.m_id
		&& m_enabled == that.m_enabled
		&& m_has_run_script == that.m_has_run_script
		&& m_has_on_script == that.m_has_on_script
		&& m_has_off_script == that.m_has_off_script
		&& m_has_change_script == that.m_has_change_script
		&& m_description == that.m_description
		&& m_comment == that.m_comment
		&& m_parameter == that.m_parameter;
}


//-------------------------------------------------
//	normalize_tag - drop the initial colon, if
//	present
//-------------------------------------------------

static void normalize_tag(QString &tag)
{
	if (tag.size() > 0 && tag[0] == ':')
		tag = tag.right(tag.length() - 1);
}


//**************************************************************************
//  STATUS UPDATES
//**************************************************************************

//-------------------------------------------------
//  update ctor
//-------------------------------------------------

status::update::update()
{
}


//-------------------------------------------------
//  update dtor
//-------------------------------------------------

status::update::~update()
{
}


//-------------------------------------------------
//  update::read()
//-------------------------------------------------

status::update status::update::read(QIODevice &input_stream)
{
	status::update result;

	XmlParser xml;
	xml.onElementBegin({ "status" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.get("phase",					result.m_phase, s_machine_phase_parser);
		attributes.get("paused",				result.m_paused);
		attributes.get("polling_input_seq",		result.m_polling_input_seq);
		attributes.get("has_input_using_mouse",	result.m_has_input_using_mouse);
		attributes.get("startup_text",			result.m_startup_text);
		attributes.get("debugger_present",		result.m_debugger_present);
	});
	xml.onElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.get("speed_percent",			result.m_speed_percent);
		attributes.get("frameskip",				result.m_frameskip);
		attributes.get("effective_frameskip",	result.m_effective_frameskip);
		attributes.get("throttled",				result.m_throttled);
		attributes.get("throttle_rate",			result.m_throttle_rate);
		attributes.get("is_recording",			result.m_is_recording);
	});
	xml.onElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.get("attenuation",			result.m_sound_attenuation);
	});
	xml.onElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images.emplace();
	});
	xml.onElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = result.m_images.value().emplace_back();
		attributes.get("tag",					image.m_tag);
		attributes.get("instance_name",			image.m_instance_name);
		attributes.get("is_readable",			image.m_is_readable, false);
		attributes.get("is_writeable",			image.m_is_writeable, false);
		attributes.get("is_creatable",			image.m_is_creatable, false);
		attributes.get("must_be_loaded",		image.m_must_be_loaded, false);
		attributes.get("filename",				image.m_file_name);
		attributes.get("display",				image.m_display);
		normalize_tag(image.m_tag);
	});
	xml.onElementBegin({ "status", "slots" }, [&](const XmlParser::Attributes &)
	{
		result.m_slots.emplace();
	});
	xml.onElementEnd({ "status", "slots" }, [&](QString &&content)
	{
		// downstream logic becomes much simpler if this is sorted
		std::sort(
			result.m_slots->begin(),
			result.m_slots->end(),
			[](const status::slot &x, const status::slot &y)
			{
				return x.m_name < y.m_name;
			});
	});
	xml.onElementBegin({ "status", "slots", "slot" }, [&](const XmlParser::Attributes &attributes)
	{
		slot &slot = result.m_slots.value().emplace_back();
		attributes.get("name",					slot.m_name);
		attributes.get("current_option",		slot.m_current_option);
		attributes.get("fixed",					slot.m_fixed, false);
		normalize_tag(slot.m_name);
	});
	xml.onElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs.emplace();
	});
	xml.onElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		input &input = result.m_inputs.value().emplace_back();
		attributes.get("port_tag",				input.m_port_tag);
		attributes.get("name",					input.m_name);
		attributes.get("mask",					input.m_mask);
		attributes.get("class",					input.m_class, s_input_class_parser);
		attributes.get("group",					input.m_group);
		attributes.get("player",				input.m_player);
		attributes.get("type",					input.m_type);
		attributes.get("is_analog",				input.m_is_analog);
		attributes.get("first_keyboard_code",	input.m_first_keyboard_code);
		attributes.get("value",					input.m_value);
		normalize_tag(input.m_port_tag);
	});
	xml.onElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		input_seq &seq = util::last(result.m_inputs.value()).m_seqs.emplace_back();
		attributes.get("type",					seq.m_type, s_inputseq_type_parser);
		attributes.get("tokens",				seq.m_tokens);
	});
	xml.onElementBegin({ "status", "input_devices" }, [&](const XmlParser::Attributes &)
	{
		result.m_input_classes.emplace();
	});
	xml.onElementBegin({ "status", "input_devices", "class" }, [&](const XmlParser::Attributes &attributes)
	{
		input_class &input_class = result.m_input_classes.value().emplace_back();
		attributes.get("name",					input_class.m_name);
		attributes.get("enabled",				input_class.m_enabled);
		attributes.get("multi",					input_class.m_multi);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device &input_device = result.m_input_classes.value().back().m_devices.emplace_back();
		attributes.get("name",					input_device.m_name);
		attributes.get("id",					input_device.m_id);
		attributes.get("devindex",				input_device.m_index);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device_item &item = result.m_input_classes.value().back().m_devices.back().m_items.emplace_back();
		attributes.get("name",					item.m_name);
		attributes.get("token",					item.m_token);
		attributes.get("code",					item.m_code);
	});
	xml.onElementBegin({ "status", "cheats" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_cheats.emplace();
	});
	xml.onElementBegin({ "status", "cheats", "cheat" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = result.m_cheats.value().emplace_back();
		attributes.get("id",					cheat.m_id);
		attributes.get("enabled",				cheat.m_enabled);
		attributes.get("has_run_script",		cheat.m_has_run_script);
		attributes.get("has_on_script",			cheat.m_has_on_script);
		attributes.get("has_off_script",		cheat.m_has_off_script);
		attributes.get("has_change_script",		cheat.m_has_change_script);
		attributes.get("description",			cheat.m_description);
		attributes.get("comment",				cheat.m_comment);
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = util::last(result.m_cheats.value());
		cheat_parameter &param = cheat.m_parameter.emplace();
		attributes.get("value",					param.m_value);
		attributes.get("minimum",				param.m_minimum);
		attributes.get("maximum",				param.m_maximum);
		attributes.get("step",					param.m_step);
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = util::last(result.m_cheats.value());
		cheat_parameter &param = cheat.m_parameter.value();
		std::uint64_t value;
		QString text;
		attributes.get("value",					value);
		attributes.get("text",					text);
		param.m_items[value] = text;
	});

	// parse the XML
	result.m_success = xml.parse(input_stream);

	// this should not happen unless there is a bug
	if (!result.m_success)
		result.m_parse_error = xml.errorMessage();

	// sort the results
	if (result.m_images)
	{
		std::sort(result.m_images.value().begin(), result.m_images.value().end(), [](const status::image &x, const status::image &y)
		{
			return x.m_tag < y.m_tag;
		});
	}

	// and return them
	return result;
}


//**************************************************************************
//  STATUS STATE
//**************************************************************************

//-------------------------------------------------
//  state ctor
//-------------------------------------------------

status::state::state()
	: m_throttled(false)
	, m_is_recording(false)
{
}


//-------------------------------------------------
//  state dtor
//-------------------------------------------------

status::state::~state()
{
}


//-------------------------------------------------
//  state::update()
//-------------------------------------------------

void status::state::update(status::update &&that)
{
	take(m_phase,					that.m_phase);
	take(m_paused,					that.m_paused);
	take(m_polling_input_seq,		that.m_polling_input_seq);
	take(m_has_input_using_mouse,	that.m_has_input_using_mouse);
	take(m_startup_text,			that.m_startup_text);
	take(m_debugger_present,		that.m_debugger_present);
	take(m_speed_percent,			that.m_speed_percent);
	take(m_frameskip,				that.m_frameskip);
	take(m_effective_frameskip,		that.m_effective_frameskip);
	take(m_throttled,				that.m_throttled);
	take(m_throttle_rate,			that.m_throttle_rate);
	take(m_is_recording,			that.m_is_recording);
	take(m_sound_attenuation,		that.m_sound_attenuation);
	take(m_images,					that.m_images);
	take(m_slots,					that.m_slots);
	take(m_inputs,					that.m_inputs);
	take(m_input_classes,			that.m_input_classes);
	take(m_cheats,					that.m_cheats);
}


//-------------------------------------------------
//  state::take()
//-------------------------------------------------

template<typename TStateField, typename TUpdateField>
bool status::state::take(TStateField &state_field, std::optional<TUpdateField> &update_field)
{
	bool result = update_field.has_value();
	if (result)
	{
		TUpdateField extracted_value = std::move(update_field.value());
		state_field = std::move(extracted_value);
	}
	return result;
}


//-------------------------------------------------
//  status::state::find_image()
//-------------------------------------------------

const status::image *status::state::find_image(const QString &tag) const
{
	return util::find_if_ptr(m_images.get(), [&tag](const status::image &image)
	{
		return image.m_tag == tag;
	});
}


//-------------------------------------------------
//  status::state::has_input_class()
//-------------------------------------------------

bool status::state::has_input_class(status::input::input_class input_class) const
{
	const status::input *input = util::find_if_ptr(m_inputs.get(), [input_class](const auto &input)
	{
		return input.m_class == input_class;
	});
	return input != nullptr;
}
