/***************************************************************************

    status.cpp

	Tracking of MAME internal state updates

***************************************************************************/

// bletchmame headers
#include "status.h"
#include "xmlparser.h"

// Qt headers
#include <QTextStream>


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


//-------------------------------------------------
//  comparePointees
//-------------------------------------------------

template<class T>
static bool comparePointees(const std::shared_ptr<T> &x, const std::shared_ptr<T> &y)
{
	return (!x && !y) || (x && y && *x == *y);
}


//-------------------------------------------------
//  image::operator==
//-------------------------------------------------

bool status::image::operator==(const status::image &that) const
{
	return m_tag == that.m_tag
		&& m_file_name == that.m_file_name
		&& comparePointees(m_details, that.m_details);
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
		const auto [phaseAttr, pausedAttr, pollingInputSeqAttr, hasInputUsingMouseAttr, hasMouseEnabledProblemAttr, startupTextAttr, debuggerPresentAttr]
			= attributes.get("phase", "paused", "polling_input_seq", "has_input_using_mouse", "has_mouse_enabled_problem", "startup_text", "debugger_present");
		rootTagParseCount++;
		result.m_phase						= phaseAttr.as<status::machine_phase>(s_machine_phase_parser);
		result.m_paused						= pausedAttr.as<bool>();
		result.m_polling_input_seq			= pollingInputSeqAttr.as<bool>();
		result.m_has_input_using_mouse		= hasInputUsingMouseAttr.as<bool>();
		result.m_has_mouse_enabled_problem	= hasMouseEnabledProblemAttr.as<bool>();
		result.m_startup_text				= startupTextAttr.as<QString>();
		result.m_debugger_present			= debuggerPresentAttr.as<bool>();
	});
	xml.onElementBegin({ "status", "video" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [speedPercentAttr, frameskipAttr, effectiveFrameskipAttr, throttledAttr, throttleRateAttr, isRecordingAttr]
			= attributes.get("speed_percent", "frameskip", "effective_frameskip", "throttled", "throttle_rate", "is_recording");
		result.m_speed_percent				= speedPercentAttr.as<float>();
		result.m_frameskip					= frameskipAttr.as<QString>();
		result.m_effective_frameskip		= effectiveFrameskipAttr.as<int>();
		result.m_throttled					= throttledAttr.as<bool>();
		result.m_throttle_rate				= throttleRateAttr.as<float>();
		result.m_is_recording				= isRecordingAttr.as<bool>();
	});
	xml.onElementBegin({ "status", "sound" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [attenuationAttr] = attributes.get("attenuation");
		result.m_sound_attenuation			= attenuationAttr.as<int>();
	});
	xml.onElementBegin({ "status", "images" }, [&](const XmlParser::Attributes &)
	{
		result.m_images.emplace();
	});
	xml.onElementBegin({ "status", "images", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [tagAttr, filenameAttr] = attributes.get("tag", "filename");
		image &image = result.m_images->emplace_back();
		image.m_tag							= tagAttr.as<QString>().value_or("");
		image.m_file_name					= filenameAttr.as<QString>().value_or("");
		normalize_tag(image.m_tag);
	});
	xml.onElementBegin({ "status", "images", "image", "details"}, [&](const XmlParser::Attributes &attributes)
	{
		const auto [instanceNameAttr, isReadableAttr, isWriteableAttr, isCreatableAttr, mustBeLoadedAttr] = attributes.get(
			"instance_name", "is_readable", "is_writeable", "is_creatable", "must_be_loaded");

		image &image = util::last(*result.m_images);
		image.m_details = std::make_shared<image_details>();
		image_details &details = *image.m_details;
		details.m_instance_name				= instanceNameAttr.as<QString>().value_or("");
		details.m_is_readable				= isReadableAttr.as<bool>().value_or(false);
		details.m_is_writeable				= isWriteableAttr.as<bool>().value_or(false);
		details.m_is_creatable				= isCreatableAttr.as<bool>().value_or(false);
		details.m_must_be_loaded			= mustBeLoadedAttr.as<bool>().value_or(false);
	});
	xml.onElementBegin({ "status", "images", "image", "details", "format" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, descriptionAttr] = attributes.get("name", "description");
		image &image = util::last(*result.m_images);
		image_format &format = image.m_details->m_formats.emplace_back();
		format.m_name						= nameAttr.as<QString>().value_or("");
		format.m_description				= descriptionAttr.as<QString>().value_or("");
	});
	xml.onElementEnd({ "status", "images", "image", "details", "format", "extension" }, [&](std::u8string &&content)
	{
		image &image = util::last(*result.m_images);
		image_format &format = util::last(image.m_details->m_formats);
		format.m_extensions.push_back(util::toQString(content));
	});
	xml.onElementBegin({ "status", "cassettes" }, [&](const XmlParser::Attributes &)
	{
		result.m_cassettes.emplace();
	});
	xml.onElementBegin({ "status", "cassettes", "cassette" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [tagAttr, isStoppedAttr, isPlayingAttr, isRecordingAttr, motorStateAttr, speakerStateAttr, positionAttr, lengthAttr]
			= attributes.get("tag", "is_stopped", "is_playing", "is_recording", "motor_state", "speaker_state", "position", "length");
		cassette &cassette = result.m_cassettes->emplace_back();
		cassette.m_tag						= tagAttr.as<QString>().value_or("");
		cassette.m_is_stopped				= isStoppedAttr.as<bool>().value_or(false);
		cassette.m_is_playing				= isPlayingAttr.as<bool>().value_or(false);
		cassette.m_is_recording				= isRecordingAttr.as<bool>().value_or(false);
		cassette.m_motor_state				= motorStateAttr.as<bool>().value_or(false);
		cassette.m_speaker_state			= speakerStateAttr.as<bool>().value_or(false);
		cassette.m_position					= positionAttr.as<float>().value_or(0.0);
		cassette.m_length					= lengthAttr.as<float>().value_or(0.0);
		normalize_tag(cassette.m_tag);
	});
	xml.onElementBegin({ "status", "slots" }, [&](const XmlParser::Attributes &)
	{
		result.m_slots.emplace();
	});
	xml.onElementEnd({ "status", "slots" }, [&]()
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
		const auto [nameAttr, currentOptionAttr, fixedAttr] = attributes.get("name", "current_option", "fixed");
		slot &slot = result.m_slots->emplace_back();
		slot.m_name							= nameAttr.as<QString>().value_or("");
		slot.m_current_option				= currentOptionAttr.as<QString>().value_or("");
		slot.m_fixed						= fixedAttr.as<bool>().value_or(false);
		normalize_tag(slot.m_name);
	});
	xml.onElementBegin({ "status", "inputs" }, [&](const XmlParser::Attributes &)
	{
		result.m_inputs.emplace();
	});
	xml.onElementBegin({ "status", "inputs", "input" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [portTagAttr, nameAttr, maskAttr, classAttr, groupAttr, playerAttr, typeAttr, isAnalogAttr, firstKeyboardCode, valueAttr]
			= attributes.get("port_tag", "name", "mask", "class", "group", "player", "type", "is_analog", "first_keyboard_code", "value");

		input &input = result.m_inputs->emplace_back();
		input.m_port_tag					= portTagAttr.as<QString>().value_or("");
		input.m_name						= nameAttr.as<QString>().value_or("");
		input.m_mask						= maskAttr.as<ioport_value>().value_or(0);
		input.m_class						= classAttr.as<status::input::input_class>(s_input_class_parser).value_or(status::input::input_class::UNKNOWN);
		input.m_group						= groupAttr.as<int>().value_or(0);
		input.m_player						= playerAttr.as<int>().value_or(0);
		input.m_type						= typeAttr.as<int>().value_or(0);
		input.m_is_analog					= isAnalogAttr.as<bool>().value_or(false);
		input.m_first_keyboard_code			= firstKeyboardCode.as<int>().value_or(0);
		input.m_value						= valueAttr.as<ioport_value>().value_or(0);
		normalize_tag(input.m_port_tag);
	});
	xml.onElementBegin({ "status", "inputs", "input", "seq" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [typeAttr, tokensAttr] = attributes.get("type", "tokens");
		input_seq &seq = util::last(*result.m_inputs).m_seqs.emplace_back();
		seq.m_type							= typeAttr.as<status::input_seq::type>(s_inputseq_type_parser).value_or(status::input_seq::type::STANDARD);
		seq.m_tokens						= tokensAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "status", "input_devices" }, [&](const XmlParser::Attributes &)
	{
		result.m_input_classes.emplace();
	});
	xml.onElementBegin({ "status", "input_devices", "class" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, enabledAttr, multiAttr] = attributes.get("name", "enabled", "multi");
		input_class &input_class = result.m_input_classes->emplace_back();
		input_class.m_name					= nameAttr.as<QString>().value_or("");
		input_class.m_enabled				= enabledAttr.as<bool>().value_or(false);
		input_class.m_multi					= multiAttr.as<bool>().value_or(false);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, idAttr, indexAttr] = attributes.get("name", "id", "devindex");
		input_device &input_device = result.m_input_classes->back().m_devices.emplace_back();
		input_device.m_name					= nameAttr.as<QString>().value_or("");
		input_device.m_id					= idAttr.as<QString>().value_or("");
		input_device.m_index				= indexAttr.as<int>().value_or(0);
	});
	xml.onElementBegin({ "status", "input_devices", "class", "device", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [nameAttr, tokenAttr, codeAttr] = attributes.get("name", "token", "code");
		input_device_item &item = result.m_input_classes->back().m_devices.back().m_items.emplace_back();
		item.m_name							= nameAttr.as<QString>().value_or("");
		item.m_token						= tokenAttr.as<QString>().value_or("");
		item.m_code							= codeAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "status", "cheats" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_cheats.emplace();
	});
	xml.onElementBegin({ "status", "cheats", "cheat" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [idAttr, enabledAttr, hasRunScriptAttr, hasOnScriptAttr, hasOffScriptAttr, hasChangeScriptAttr, descriptionAttr, commentAttr]
			= attributes.get("id", "enabled", "has_run_script", "has_on_script", "has_off_script", "has_change_script", "description", "comment");
		cheat &cheat = result.m_cheats->emplace_back();
		cheat.m_id							= idAttr.as<QString>().value_or("");
		cheat.m_enabled						= enabledAttr.as<bool>().value_or(false);
		cheat.m_has_run_script				= hasRunScriptAttr.as<bool>().value_or(false);
		cheat.m_has_on_script				= hasOnScriptAttr.as<bool>().value_or(false);
		cheat.m_has_off_script				= hasOffScriptAttr.as<bool>().value_or(false);
		cheat.m_has_change_script			= hasChangeScriptAttr.as<bool>().value_or(false);
		cheat.m_description					= descriptionAttr.as<QString>().value_or("");
		cheat.m_comment						= commentAttr.as<QString>().value_or("");
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [valueAttr, minimumAttr, maximumAttr, stepAttr] = attributes.get("value", "minimum", "maximum", "step");
		cheat &cheat = util::last(*result.m_cheats);
		cheat_parameter &param = cheat.m_parameter.emplace();
		param.m_value						= valueAttr.as<std::uint64_t>().value_or(0);
		param.m_minimum						= minimumAttr.as<std::uint64_t>().value_or(0);
		param.m_maximum						= maximumAttr.as<std::uint64_t>().value_or(0);
		param.m_step						= stepAttr.as<std::uint64_t>().value_or(0);
	});
	xml.onElementBegin({ "status", "cheats", "cheat", "parameter", "item" }, [&](const XmlParser::Attributes &attributes)
	{
		const auto [valueAttr, textAttr] = attributes.get("value", "text");
		cheat &cheat = util::last(*result.m_cheats);
		cheat_parameter &param = *cheat.m_parameter;
		std::uint64_t value					= valueAttr.as<std::uint64_t>().value_or(0);
		QString text						= textAttr.as<QString>().value_or("");
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
		std::sort(result.m_images->begin(), result.m_images->end(), [](const status::image &x, const status::image &y)
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
//  retainDetails
//-------------------------------------------------

template<class T>
static void retainDetails(std::optional<std::vector<T>> &updateVec, const std::vector<T> &stateVec)
{
	// if this update does not provide image details, use the current details
	if (updateVec)
	{
		for (T &thatItem : *updateVec)
		{
			if (!thatItem.m_details)
			{
				auto iter = std::ranges::find_if(stateVec, [&thatItem](const T &x)
				{
					return x.m_tag == thatItem.m_tag;
				});
				if (iter != stateVec.end())
					thatItem.m_details = iter->m_details;
			}
		}
	}
}


//-------------------------------------------------
//  state::update()
//-------------------------------------------------

void status::state::update(status::update &&that)
{
	// retain details about images
	retainDetails(that.m_images, m_images.get());

	// take all the things
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
	take(m_cassettes,					that.m_cassettes);
	take(m_slots,						that.m_slots);
	take(m_inputs,						that.m_inputs);
	take(m_input_classes,				that.m_input_classes);
	take(m_cheats,						that.m_cheats);
}


//-------------------------------------------------
//  state::take()
//-------------------------------------------------

template<typename TStateField, typename TUpdateField>
bool status::state::take(TStateField &state_field, TUpdateField &update_field)
{
	bool result = bool(update_field);
	if (result)
		state_field = std::move(*update_field);
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
