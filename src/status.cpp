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

static const util::enum_parser<status::input::input_type> s_input_type_parser =
{
	{ "analog", status::input::input_type::ANALOG, },
	{ "digital", status::input::input_type::DIGITAL }
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
		&& m_is_createable == that.m_is_createable
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
		&& m_text == that.m_text;
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
		&& m_type == that.m_type
		&& m_value == that.m_value
		&& m_seqs == that.m_seqs;
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
		attributes.Get("phase",				result.m_phase, s_machine_phase_parser);
		attributes.Get("paused",			result.m_paused);
		attributes.Get("polling_input_seq",	result.m_polling_input_seq);
		attributes.Get("startup_text",		result.m_startup_text);
	});
	xml.OnElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("frameskip",			result.m_frameskip);
		attributes.Get("speed_text",		result.m_speed_text);
		attributes.Get("throttled",			result.m_throttled);
		attributes.Get("throttle_rate",		result.m_throttle_rate);
	});
	xml.OnElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("attenuation",		result.m_sound_attenuation);
	});
	xml.OnElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images = std::vector<status::image>();
	});
	xml.OnElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &image = result.m_images.value().emplace_back();
		attributes.Get("tag",				image.m_tag);
		attributes.Get("instance_name",		image.m_instance_name);
		attributes.Get("is_readable",		image.m_is_readable, false);
		attributes.Get("is_writeable",		image.m_is_writeable, false);
		attributes.Get("is_createable",		image.m_is_createable, false);
		attributes.Get("must_be_loaded",	image.m_must_be_loaded, false);
		attributes.Get("filename",			image.m_file_name);
		attributes.Get("display",			image.m_display);
		normalize_tag(image.m_tag);
	});
	xml.OnElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs = std::vector<status::input>();
	});
	xml.OnElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		input &input = result.m_inputs.value().emplace_back();
		attributes.Get("port_tag",			input.m_port_tag);
		attributes.Get("mask",				input.m_mask);
		attributes.Get("class",				input.m_class, s_input_class_parser);
		attributes.Get("type",				input.m_type, s_input_type_parser);
		attributes.Get("name",				input.m_name);
		attributes.Get("value",				input.m_value);
		normalize_tag(input.m_port_tag);
	});
	xml.OnElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		input_seq &seq = util::last(result.m_inputs.value()).m_seqs.emplace_back();
		attributes.Get("type",				seq.m_type, s_inputseq_type_parser);
		attributes.Get("text",				seq.m_text);
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

	// and return it
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
	take(m_phase, that.m_phase);
	take(m_paused, that.m_paused);
	take(m_polling_input_seq, that.m_polling_input_seq);
	take(m_startup_text, that.m_startup_text);
	take(m_speed_text, that.m_speed_text);
	take(m_frameskip, that.m_frameskip);
	take(m_throttled, that.m_throttled);
	take(m_throttle_rate, that.m_throttle_rate);
	take(m_sound_attenuation, that.m_sound_attenuation);
	take(m_images, that.m_images);
	take(m_inputs, that.m_inputs);
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
	auto iter = std::find_if(
		m_images.get().begin(),
		m_images.get().end(),
		[&tag](const status::image &image) { return image.m_tag == tag; });

	return iter != m_images.get().end()
		? &*iter
		: nullptr;
}


//-------------------------------------------------
//  status::state::has_input_class()
//-------------------------------------------------

bool status::state::has_input_class(status::input::input_class input_class) const
{
	auto iter = std::find_if(
		m_inputs.get().begin(),
		m_inputs.get().end(),
		[input_class](const auto &input)
		{
			return input.m_class == input_class;
		});
	return iter != m_inputs.get().end();
}
