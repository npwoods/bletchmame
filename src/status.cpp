/***************************************************************************

    status.cpp

	Tracking of MAME internal state updates

***************************************************************************/

#include <wx/txtstrm.h>
#include <wx/sstream.h>

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
//	normalize_tag - drop the initial colon, if
//	present
//-------------------------------------------------

static void normalize_tag(wxString &tag)
{
	if (tag.size() > 0 && tag[0] == ':')
		tag = tag.substr(1);
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

status::update status::update::read(wxTextInputStream &input_stream)
{
	status::update result;

	XmlParser xml;
	xml.OnElementBegin({ "status" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("phase",					result.m_phase, s_machine_phase_parser);
		attributes.Get("paused",				result.m_paused);
		attributes.Get("polling_input_seq",		result.m_polling_input_seq);
		attributes.Get("has_input_using_mouse",	result.m_has_input_using_mouse);
		attributes.Get("startup_text",			result.m_startup_text);
		attributes.Get("debugger_present",		result.m_debugger_present);
	});
	xml.OnElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("speed_percent",			result.m_speed_percent);
		attributes.Get("frameskip",				result.m_frameskip);
		attributes.Get("effective_frameskip",	result.m_effective_frameskip);
		attributes.Get("throttled",				result.m_throttled);
		attributes.Get("throttle_rate",			result.m_throttle_rate);
	});
	xml.OnElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("attenuation",		result.m_sound_attenuation);
	});
	xml.OnElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images.emplace();
	});
	xml.OnElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = result.m_images.value().emplace_back();
		attributes.Get("tag",				image.m_tag);
		attributes.Get("instance_name",		image.m_instance_name);
		attributes.Get("is_readable",		image.m_is_readable, false);
		attributes.Get("is_writeable",		image.m_is_writeable, false);
		attributes.Get("is_creatable",		image.m_is_creatable, false);
		attributes.Get("must_be_loaded",	image.m_must_be_loaded, false);
		attributes.Get("filename",			image.m_file_name);
		attributes.Get("display",			image.m_display);
		normalize_tag(image.m_tag);
	});
	xml.OnElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs.emplace();
	});
	xml.OnElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		input &input = result.m_inputs.value().emplace_back();
		attributes.Get("port_tag",				input.m_port_tag);
		attributes.Get("name",					input.m_name);
		attributes.Get("mask",					input.m_mask);
		attributes.Get("class",					input.m_class, s_input_class_parser);
		attributes.Get("group",					input.m_group);
		attributes.Get("player",				input.m_player);
		attributes.Get("type",					input.m_type);
		attributes.Get("is_analog",				input.m_is_analog);
		attributes.Get("first_keyboard_code",	input.m_first_keyboard_code);
		attributes.Get("value",					input.m_value);
		normalize_tag(input.m_port_tag);
	});
	xml.OnElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		input_seq &seq = util::last(result.m_inputs.value()).m_seqs.emplace_back();
		attributes.Get("type",				seq.m_type, s_inputseq_type_parser);
		attributes.Get("tokens",			seq.m_tokens);
	});
	xml.OnElementBegin({ "status", "input_devices" }, [&](const XmlParser::Attributes &)
	{
		result.m_input_classes.emplace();
	});
	xml.OnElementBegin({ "status", "input_devices", "class" }, [&](const XmlParser::Attributes &attributes)
	{
		input_class &input_class = result.m_input_classes.value().emplace_back();
		attributes.Get("name",				input_class.m_name);
		attributes.Get("enabled",			input_class.m_enabled);
		attributes.Get("multi",				input_class.m_multi);
	});
	xml.OnElementBegin({ "status", "input_devices", "class", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device &input_device = result.m_input_classes.value().back().m_devices.emplace_back();
		attributes.Get("name",				input_device.m_name);
		attributes.Get("id",				input_device.m_id);
		attributes.Get("devindex",			input_device.m_index);
	});
	xml.OnElementBegin({ "status", "input_devices", "class", "device", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		input_device_item &item = result.m_input_classes.value().back().m_devices.back().m_items.emplace_back();
		attributes.Get("name",				item.m_name);
		attributes.Get("token",				item.m_token);
		attributes.Get("code",				item.m_code);
	});

	// because XmlParser::Parse() is not smart enough to read until XML ends, we are using this
	// crude mechanism to read the XML
	wxString buffer;
	bool done = false;
	while (!done)
	{
		wxString line = input_stream.ReadLine();
		buffer.Append(line);

		if (input_stream.GetInputStream().Eof() || line.StartsWith("</"))
			done = true;
	}

	// parse the XML
	wxStringInputStream input_buffer(buffer);
	result.m_success = xml.Parse(input_buffer);

	// this should not happen unless there is a bug
	if (!result.m_success)
		result.m_parse_error = xml.ErrorMessage();

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
	take(m_sound_attenuation,		that.m_sound_attenuation);
	take(m_images,					that.m_images);
	take(m_inputs,					that.m_inputs);
	take(m_input_classes,			that.m_input_classes);
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

const status::image *status::state::find_image(const wxString &tag) const
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
