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
	int rootTagParseCount = 0;
	status::update result;

	XmlParser xml;
	xml.onElementBegin({ "status" }, [&](const XmlParser::Attributes &attributes)
	{
		rootTagParseCount++;
		result.m_phase						= attributes.get<status::machine_phase>("phase", s_machine_phase_parser);
		result.m_paused						= attributes.get<bool>("paused");
		result.m_polling_input_seq			= attributes.get<bool>("polling_input_seq");
		result.m_has_input_using_mouse		= attributes.get<bool>("has_input_using_mouse");
		result.m_has_mouse_enabled_problem	= attributes.get<bool>("has_mouse_enabled_problem");
		result.m_startup_text				= attributes.get<QString>("startup_text");
		result.m_debugger_present			= attributes.get<bool>("debugger_present");
	});
	xml.onElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_speed_percent				= attributes.get<float>("speed_percent");
		result.m_frameskip					= attributes.get<QString>("frameskip");
		result.m_effective_frameskip		= attributes.get<int>("effective_frameskip");
		result.m_throttled					= attributes.get<bool>("throttled");
		result.m_throttle_rate				= attributes.get<float>("throttle_rate");
		result.m_is_recording				= attributes.get<bool>("is_recording");
	});
	xml.onElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_sound_attenuation			= attributes.get<int>("attenuation");
	});
	xml.onElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images.emplace();
	});
	xml.onElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = result.m_images.value().emplace_back();
		image.m_tag							= attributes.get<QString>("tag").value_or("");
		image.m_instance_name				= attributes.get<QString>("instance_name").value_or("");
		image.m_is_readable					= attributes.get<bool>("is_readable").value_or(false);
		image.m_is_writeable				= attributes.get<bool>("is_writeable").value_or(false);
		image.m_is_creatable				= attributes.get<bool>("is_creatable").value_or(false);
		image.m_must_be_loaded				= attributes.get<bool>("must_be_loaded").value_or(false);
		image.m_file_name					= attributes.get<QString>("filename").value_or("");
		image.m_display						= attributes.get<QString>("display").value_or("");
		normalize_tag(image.m_tag);
	});
	xml.onElementBegin({ "status", "images", "image", "formats" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = util::last(*result.m_images);
		image.m_formats.emplace();
	});
	xml.onElementBegin({ "status", "images", "image", "formats", "format" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = util::last(*result.m_images);
		image_format &format = image.m_formats.value().emplace_back();
		format.m_name						= attributes.get<QString>("name").value_or("");
		format.m_description				= attributes.get<QString>("description").value_or("");
		format.m_option_spec				= attributes.get<QString>("option_spec").value_or("");
	});
	xml.onElementEnd({ "status", "images", "image", "formats", "format", "extension" }, [&](QString &&content)
	{
		image &image = util::last(*result.m_images);
		image_format &format = util::last(*image.m_formats);
		format.m_extensions.push_back(std::move(content));
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
		slot.m_name							= attributes.get<QString>("name").value_or("");
		slot.m_current_option				= attributes.get<QString>("current_option").value_or("");
		slot.m_fixed						= attributes.get<bool>("fixed").value_or(false);
		normalize_tag(slot.m_name);
	});
	xml.onElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs.emplace();
	});
	xml.onElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		input &input = result.m_inputs.value().emplace_back();
		input.m_port_tag					= attributes.get<QString>("port_tag").value_or("");
		input.m_name						= attributes.get<QString>("name").value_or("");
		input.m_mask						= attributes.get<ioport_value>("mask").value_or(0);
		input.m_class						= attributes.get<status::input::input_class>("class", s_input_class_parser).value_or(status::input::input_class::UNKNOWN);
		input.m_group						= attributes.get<int>("group").value_or(0);
		input.m_player						= attributes.get<int>("player").value_or(0);
		input.m_type						= attributes.get<int>("type").value_or(0);
		input.m_is_analog					= attributes.get<bool>("is_analog").value_or(false);
		input.m_first_keyboard_code			= attributes.get<int>("first_keyboard_code").value_or(0);
		input.m_value						= attributes.get<ioport_value>("value").value_or(0);
		normalize_tag(input.m_port_tag);
	});
	xml.onElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		input_seq &seq = util::last(result.m_inputs.value()).m_seqs.emplace_back();
		seq.m_type							= attributes.get<status::input_seq::type>("type", s_inputseq_type_parser).value_or(status::input_seq::type::STANDARD);
		seq.m_tokens						= attributes.get<QString>("tokens").value_or("");
	});
	xml.onElementBegin({ "status", "input_devices" }, [&](const XmlParser::Attributes &)
	{
		result.m_input_classes.emplace();
	});
	xml.onElementBegin({ "status", "input_devices", "class" }, [&](const XmlParser::Attributes &attributes)
	{
		input_class &input_class = result.m_input_classes.value().emplace_back();
		input_class.m_name					= attributes.get<QString>("name").value_or("");
		input_class.m_enabled				= attributes.get<bool>("enabled").value_or(false);
		input_class.m_multi					= attributes.get<bool>("multi").value_or(false);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device &input_device = result.m_input_classes.value().back().m_devices.emplace_back();
		input_device.m_name					= attributes.get<QString>("name").value_or("");
		input_device.m_id					= attributes.get<QString>("id").value_or("");
		input_device.m_index				= attributes.get<int>("devindex").value_or(0);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device_item &item = result.m_input_classes.value().back().m_devices.back().m_items.emplace_back();
		item.m_name							= attributes.get<QString>("name").value_or("");
		item.m_token						= attributes.get<QString>("token").value_or("");
		item.m_code							= attributes.get<QString>("code").value_or("");
	});
	xml.onElementBegin({ "status", "cheats" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_cheats.emplace();
	});
	xml.onElementBegin({ "status", "cheats", "cheat" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = result.m_cheats.value().emplace_back();
		cheat.m_id							= attributes.get<QString>("id").value_or("");
		cheat.m_enabled						= attributes.get<bool>("enabled").value_or(false);
		cheat.m_has_run_script				= attributes.get<bool>("has_run_script").value_or(false);
		cheat.m_has_on_script				= attributes.get<bool>("has_on_script").value_or(false);
		cheat.m_has_off_script				= attributes.get<bool>("has_off_script").value_or(false);
		cheat.m_has_change_script			= attributes.get<bool>("has_change_script").value_or(false);
		cheat.m_description					= attributes.get<QString>("description").value_or("");
		cheat.m_comment						= attributes.get<QString>("comment").value_or("");
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = util::last(result.m_cheats.value());
		cheat_parameter &param = cheat.m_parameter.emplace();
		param.m_value						= attributes.get<std::uint64_t>("value").value_or(0);
		param.m_minimum						= attributes.get<std::uint64_t>("minimum").value_or(0);
		param.m_maximum						= attributes.get<std::uint64_t>("maximum").value_or(0);
		param.m_step						= attributes.get<std::uint64_t>("step").value_or(0);
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		cheat &cheat = util::last(result.m_cheats.value());
		cheat_parameter &param = cheat.m_parameter.value();
		std::uint64_t value					= attributes.get<std::uint64_t>("value").value_or(0);
		QString text						= attributes.get<QString>("text").value_or("");
		param.m_items[value] = std::move(text);
	});

	// parse the XML
	result.m_success = xml.parse(input_stream);

	// this should not happen unless there is a bug
	if (!result.m_success)
		result.m_parse_error = xml.errorMessagesSingleString();

	// check that we parsed the status tag once
	if (result.m_success && rootTagParseCount != 1)
	{
		result.m_success = false;
		result.m_parse_error = "Could not parse <status> tag";
	}

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
	: m_paused(false)
	, m_polling_input_seq(false)
	, m_has_input_using_mouse(false)
	, m_has_mouse_enabled_problem(false)
	, m_debugger_present(false)
	, m_speed_percent(0)
	, m_effective_frameskip(0)
	, m_throttled(false)
	, m_is_recording(false)
	, m_sound_attenuation(0)
	, m_cheats_enabled(false)
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
	take(m_phase,						that.m_phase);
	take(m_paused,						that.m_paused);
	take(m_polling_input_seq,			that.m_polling_input_seq);
	take(m_has_input_using_mouse,		that.m_has_input_using_mouse);
	take(m_has_mouse_enabled_problem,	that.m_has_mouse_enabled_problem);
	take(m_startup_text,				that.m_startup_text);
	take(m_debugger_present,			that.m_debugger_present);
	take(m_speed_percent,				that.m_speed_percent);
	take(m_frameskip,					that.m_frameskip);
	take(m_effective_frameskip,			that.m_effective_frameskip);
	take(m_throttled,					that.m_throttled);
	take(m_throttle_rate,				that.m_throttle_rate);
	take(m_is_recording,				that.m_is_recording);
	take(m_sound_attenuation,			that.m_sound_attenuation);
	take(m_images,						that.m_images);
	take(m_slots,						that.m_slots);
	take(m_inputs,						that.m_inputs);
	take(m_input_classes,				that.m_input_classes);
	take(m_cheats,						that.m_cheats);
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
