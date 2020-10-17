/***************************************************************************

	info_builder.cpp

	Code to build MAME info DB

***************************************************************************/

#include "info_builder.h"
#include "xmlparser.h"


//**************************************************************************
//  LOCALS
//**************************************************************************

static const util::enum_parser<info::software_list::status_type> s_status_parser =
{
	{ "original", info::software_list::status_type::ORIGINAL, },
	{ "compatible", info::software_list::status_type::COMPATIBLE }
};


static const util::enum_parser<info::configuration_condition::relation_t> s_relation_parser =
{
	{ "eq", info::configuration_condition::relation_t::EQ },
	{ "ne", info::configuration_condition::relation_t::NE },
	{ "gt", info::configuration_condition::relation_t::GT },
	{ "le", info::configuration_condition::relation_t::LE },
	{ "lt", info::configuration_condition::relation_t::LT },
	{ "ge", info::configuration_condition::relation_t::GE }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  to_uint32
//-------------------------------------------------

template<typename T>
static std::uint32_t to_uint32(T &&value)
{
	std::uint32_t new_value = static_cast<std::uint32_t>(value);
	if (new_value != value)
		throw std::logic_error("Array size cannot fit in 32 bits");
	return new_value;
};


//-------------------------------------------------
//  writeVectorData
//-------------------------------------------------

template<typename T>
static void writeVectorData(QIODevice &stream, const std::vector<T> &vector)
{
	stream.write((const char *)vector.data(), vector.size() * sizeof(T));
}


//-------------------------------------------------
//  process_xml()
//-------------------------------------------------

bool info::database_builder::process_xml(QDataStream &input, QString &error_message)
{
	// sanity check; ensure we're fresh
	assert(m_machines.empty());
	assert(m_devices.empty());

	// prepare data
	info::binaries::header header = { 0, };

	// reserve space based on what we know about MAME 0.213
	m_machines.reserve(40000);					// 36111 machines
	m_devices.reserve(9000);					// 8211 devices
	m_configurations.reserve(500000);			// 474840 configurations
	m_configuration_conditions.reserve(6000);	// 5910 conditions
	m_configuration_settings.reserve(1500000);	// 1454273 settings
	m_software_lists.reserve(4200);				// 3977 software lists
	m_ram_options.reserve(3800);				// 3616 ram options

	// header magic variables
	header.m_size_header					= sizeof(info::binaries::header);
	header.m_size_machine					= sizeof(info::binaries::machine);
	header.m_size_device					= sizeof(info::binaries::device);
	header.m_size_configuration				= sizeof(info::binaries::configuration);
	header.m_size_configuration_setting		= sizeof(info::binaries::configuration_setting);
	header.m_size_configuration_condition	= sizeof(info::binaries::configuration_condition);
	header.m_size_software_list				= sizeof(info::binaries::software_list);
	header.m_size_ram_option				= sizeof(info::binaries::ram_option);

	// parse the -listxml output
	XmlParser xml;
	std::string current_device_extensions;
	xml.onElementBegin({ "mame" }, [this, &header](const XmlParser::Attributes &attributes)
	{
		std::string build;
		header.m_build_strindex = attributes.get("build", build) ? m_strings.get(build) : 0;		
	});
	xml.onElementBegin({ "mame", "machine" }, [this](const XmlParser::Attributes &attributes)
	{
		bool runnable;
		if (attributes.get("runnable", runnable) && !runnable)
			return XmlParser::ElementResult::Skip;

		std::string data;
		info::binaries::machine &machine = m_machines.emplace_back();
		machine.m_name_strindex			= attributes.get("name", data) ? m_strings.get(data) : 0;
		machine.m_sourcefile_strindex	= attributes.get("sourcefile", data) ? m_strings.get(data) : 0;
		machine.m_clone_of_strindex		= attributes.get("cloneof", data) ? m_strings.get(data) : 0;
		machine.m_rom_of_strindex		= attributes.get("romof", data) ? m_strings.get(data) : 0;
		machine.m_configurations_index	= to_uint32(m_configurations.size());
		machine.m_configurations_count	= 0;
		machine.m_software_lists_index	= to_uint32(m_software_lists.size());
		machine.m_software_lists_count	= 0;
		machine.m_ram_options_index		= to_uint32(m_ram_options.size());
		machine.m_ram_options_count		= 0;
		machine.m_devices_index			= to_uint32(m_devices.size());
		machine.m_devices_count			= 0;
		machine.m_description_strindex	= 0;
		machine.m_year_strindex			= 0;
		machine.m_manufacturer_strindex = 0;
		return XmlParser::ElementResult::Ok;
	});
	xml.onElementEnd({ "mame", "machine", "description" }, [this](QString &&content)
	{
		util::last(m_machines).m_description_strindex = m_strings.get(content);
	});
	xml.onElementEnd({ "mame", "machine", "year" }, [this](QString &&content)
	{
		util::last(m_machines).m_year_strindex = m_strings.get(content);
	});
	xml.onElementEnd({ "mame", "machine", "manufacturer" }, [this](QString &&content)
	{
		util::last(m_machines).m_manufacturer_strindex = m_strings.get(content);
	});
	xml.onElementBegin({ { "mame", "machine", "configuration" },
						 { "mame", "machine", "dipswitch" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration &configuration = m_configurations.emplace_back();
		configuration.m_name_strindex					= attributes.get("name", data) ? m_strings.get(data) : 0;
		configuration.m_tag_strindex					= attributes.get("tag", data) ? m_strings.get(data) : 0;
		configuration.m_configuration_settings_index	= to_uint32(m_configuration_settings.size());
		configuration.m_configuration_settings_count	= 0;
		attributes.get("mask", configuration.m_mask);
	
		util::last(m_machines).m_configurations_count++;
	});
	xml.onElementBegin({ { "mame", "machine", "configuration", "confsetting" },
						 { "mame", "machine", "dipswitch", "dipvalue" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_setting &configuration_setting = m_configuration_settings.emplace_back();
		configuration_setting.m_name_strindex		= attributes.get("name", data) ? m_strings.get(data) : 0;
		configuration_setting.m_conditions_index	= to_uint32(m_configuration_conditions.size());
		attributes.get("value", configuration_setting.m_value);

		util::last(m_configurations).m_configuration_settings_count++;
	});
	xml.onElementBegin({ { "mame", "machine", "configuration", "confsetting", "condition" },
						 { "mame", "machine", "dipswitch", "dipvalue", "condition" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_condition &configuration_condition = m_configuration_conditions.emplace_back();
		configuration_condition::relation_t relation;
		configuration_condition.m_tag_strindex			= attributes.get("tag", data) ? m_strings.get(data) : 0;
		configuration_condition.m_relation				= (uint8_t)(attributes.get("relation", relation, s_relation_parser) ? relation : info::configuration_condition::relation_t::UNKNOWN);
		attributes.get("mask", configuration_condition.m_mask);
		attributes.get("value", configuration_condition.m_value);
	});
	xml.onElementBegin({ "mame", "machine", "device" }, [this, &current_device_extensions](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool mandatory;
		info::binaries::device &device = m_devices.emplace_back();
		device.m_type_strindex			= attributes.get("type", data) ? m_strings.get(data) : 0;
		device.m_tag_strindex			= attributes.get("tag", data) ? m_strings.get(data) : 0;
		device.m_interface_strindex		= attributes.get("interface", data) ? m_strings.get(data) : 0;
		device.m_mandatory				= attributes.get("mandatory", mandatory) && mandatory ? 1 : 0;
		device.m_instance_name_strindex	= 0;
		device.m_extensions_strindex	= 0;

		current_device_extensions.clear();

		util::last(m_machines).m_devices_count++;
	});
	xml.onElementBegin({ "mame", "machine", "device", "instance" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		if (attributes.get("name", data))
			util::last(m_devices).m_instance_name_strindex = m_strings.get(data);
	});
	xml.onElementBegin({ "mame", "machine", "device", "extension" }, [this, &current_device_extensions](const XmlParser::Attributes &attributes)
	{
		std::string name;
		if (attributes.get("name", name))
		{
			current_device_extensions.append(name);
			current_device_extensions.append(",");
		}
	});
	xml.onElementEnd({ "mame", "machine", "device" }, [this, &current_device_extensions](QString &&)
	{
		if (!current_device_extensions.empty())
			util::last(m_devices).m_extensions_strindex = m_strings.get(current_device_extensions);
	});
	xml.onElementBegin({ "mame", "machine", "softwarelist" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::software_list::status_type status;
		info::binaries::software_list &software_list = m_software_lists.emplace_back();
		software_list.m_name_strindex			= attributes.get("name", data) ? m_strings.get(data) : 0;
		software_list.m_filter_strindex			= attributes.get("filter", data) ? m_strings.get(data) : 0;
		software_list.m_status					= (uint8_t) (attributes.get("status", status, s_status_parser) ? status : info::software_list::status_type::ORIGINAL);
		util::last(m_machines).m_software_lists_count++;
	});
	xml.onElementBegin({ "mame", "machine", "ramoption" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool b;
		info::binaries::ram_option &ram_option = m_ram_options.emplace_back();
		ram_option.m_name_strindex				= attributes.get("name", data) ? m_strings.get(data) : 0;
		ram_option.m_is_default					= attributes.get("default", b) && b;
		ram_option.m_value						= 0;
		util::last(m_machines).m_ram_options_count++;
	});
	xml.onElementEnd({ "mame", "machine", "ramoption" }, [this](QString &&content)
	{
		bool ok;
		unsigned long val = content.toULong(&ok);
		util::last(m_ram_options).m_value = ok ? val : 0;
	});

	// parse!
	bool success;
	try
	{
		success = xml.parse(input);
	}
	catch (std::exception &ex)
	{
		// did an exception (probably thrown by to_uint32) get thrown?
		error_message = ex.what();
		return false;
	}
	if (!success)
	{
		// now check for XML parsing errors; this is likely the result of somebody aborting the DB rebuild, but
		// it is the caller's responsibility to handle that situation
		error_message = xml.errorMessage();
		return false;
	}

	// final magic bytes on string table
	m_strings.embed_value(info::binaries::MAGIC_STRINGTABLE_END);

	// finalize the header
	header.m_machines_count					= to_uint32(m_machines.size());
	header.m_devices_count					= to_uint32(m_devices.size());
	header.m_configurations_count			= to_uint32(m_configurations.size());
	header.m_configuration_settings_count	= to_uint32(m_configuration_settings.size());
	header.m_configuration_conditions_count	= to_uint32(m_configuration_conditions.size());
	header.m_software_lists_count			= to_uint32(m_software_lists.size());
	header.m_ram_options_count				= to_uint32(m_ram_options.size());

	// and salt it
	m_salted_header = util::salt(header, info::binaries::salt());

	// success!
	error_message.clear();
	return true;
}


//-------------------------------------------------
//  emit_info
//-------------------------------------------------

void info::database_builder::emit_info(QIODevice &output) const
{
	output.write((const char *) &m_salted_header, sizeof(m_salted_header));
	writeVectorData(output, m_machines);
	writeVectorData(output, m_devices);
	writeVectorData(output, m_configurations);
	writeVectorData(output, m_configuration_settings);
	writeVectorData(output, m_configuration_conditions);
	writeVectorData(output, m_software_lists);
	writeVectorData(output, m_ram_options);
	writeVectorData(output, m_strings.data());
}


//-------------------------------------------------
//  string_table ctor
//-------------------------------------------------

info::database_builder::string_table::string_table()
{
	// reserve space based on expected size (see comments above)
	m_data.reserve(2400000);		// 2001943 bytes
	m_map.reserve(105000);			// 96686 entries

	// special case; prime empty string to be #0
	get(std::string());

	// embed the initial magic bytes
	embed_value(info::binaries::MAGIC_STRINGTABLE_BEGIN);
}


//-------------------------------------------------
//  string_table::get
//-------------------------------------------------

std::uint32_t info::database_builder::string_table::get(const std::string &s)
{
	// if we've already cached this value, look it up
	auto iter = m_map.find(s);
	if (iter != m_map.end())
		return iter->second;

	// we're going to append the string; the current size becomes the position of the new string
	std::uint32_t result = to_uint32(m_data.size());

	// append the string (including trailing NUL) to m_data
	m_data.insert(m_data.end(), s.c_str(), s.c_str() + s.size() + 1);

	// and to m_map
	m_map[s] = result;

	// and return
	return result;
}


std::uint32_t info::database_builder::string_table::get(const QString &s)
{
	return get(s.toStdString());
}


//-------------------------------------------------
//  string_table::data
//-------------------------------------------------

const std::vector<char> &info::database_builder::string_table::data() const
{
	return m_data;
}
