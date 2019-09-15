/***************************************************************************

	info_builder.cpp

	Code to build MAME info DB

***************************************************************************/

#include "info_builder.h"
#include "xmlparser.h"


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
//  process_xml()
//-------------------------------------------------

bool info::database_builder::process_xml(wxInputStream &input, wxString &error_message)
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

	// header magic variables
	header.m_size_header					= sizeof(info::binaries::header);
	header.m_size_machine					= sizeof(info::binaries::machine);
	header.m_size_device					= sizeof(info::binaries::device);
	header.m_size_configuration				= sizeof(info::binaries::configuration);
	header.m_size_configuration_setting		= sizeof(info::binaries::configuration_setting);
	header.m_size_configuration_condition	= sizeof(info::binaries::configuration_condition);

	// parse the -listxml output
	XmlParser xml;
	std::string current_device_extensions;
	xml.OnElementBegin({ "mame" }, [this, &header](const XmlParser::Attributes &attributes)
	{
		std::string build;
		header.m_build_strindex = attributes.Get("build", build) ? m_strings.get(build) : 0;		
	});
	xml.OnElementBegin({ "mame", "machine" }, [this](const XmlParser::Attributes &attributes)
	{
		bool runnable;
		if (attributes.Get("runnable", runnable) && !runnable)
			return XmlParser::element_result::SKIP;

		std::string data;
		info::binaries::machine &machine = m_machines.emplace_back();
		machine.m_name_strindex			= attributes.Get("name", data) ? m_strings.get(data) : 0;
		machine.m_sourcefile_strindex	= attributes.Get("sourcefile", data) ? m_strings.get(data) : 0;
		machine.m_clone_of_strindex		= attributes.Get("cloneof", data) ? m_strings.get(data) : 0;
		machine.m_rom_of_strindex		= attributes.Get("romof", data) ? m_strings.get(data) : 0;
		machine.m_configurations_index	= to_uint32(m_configurations.size());
		machine.m_configurations_count	= 0;
		machine.m_devices_index			= to_uint32(m_devices.size());
		machine.m_devices_count			= 0;
		machine.m_description_strindex	= 0;
		machine.m_year_strindex			= 0;
		machine.m_manufacturer_strindex = 0;
		return XmlParser::element_result::OK;
	});
	xml.OnElementEnd({ "mame", "machine", "description" }, [this](wxString &&content)
	{
		util::last(m_machines).m_description_strindex = m_strings.get(content);
	});
	xml.OnElementEnd({ "mame", "machine", "year" }, [this](wxString &&content)
	{
		util::last(m_machines).m_year_strindex = m_strings.get(content);
	});
	xml.OnElementEnd({ "mame", "machine", "manufacturer" }, [this](wxString &&content)
	{
		util::last(m_machines).m_manufacturer_strindex = m_strings.get(content);
	});
	xml.OnElementBegin({ { "mame", "machine", "configuration" },
						 { "mame", "machine", "dipswitch" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration &configuration = m_configurations.emplace_back();
		configuration.m_name_strindex					= attributes.Get("name", data) ? m_strings.get(data) : 0;
		configuration.m_tag_strindex					= attributes.Get("tag", data) ? m_strings.get(data) : 0;
		configuration.m_configuration_settings_index	= to_uint32(m_configuration_settings.size());
		configuration.m_configuration_settings_count	= 0;
		attributes.Get("mask", configuration.m_mask);
	
		util::last(m_machines).m_configurations_count++;
	});
	xml.OnElementBegin({ { "mame", "machine", "configuration", "confsetting" },
						 { "mame", "machine", "dipswitch", "dipvalue" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_setting &configuration_setting = m_configuration_settings.emplace_back();
		configuration_setting.m_name_strindex		= attributes.Get("name", data) ? m_strings.get(data) : 0;
		configuration_setting.m_conditions_index	= to_uint32(m_configuration_conditions.size());
		attributes.Get("value", configuration_setting.m_value);

		util::last(m_configurations).m_configuration_settings_count++;
	});
	xml.OnElementBegin({ { "mame", "machine", "configuration", "confsetting", "condition" },
						 { "mame", "machine", "dipswitch", "dipvalue", "condition" } }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_condition &configuration_condition = m_configuration_conditions.emplace_back();
		configuration_condition.m_tag_strindex			= attributes.Get("tag", data) ? m_strings.get(data) : 0;
		configuration_condition.m_relation_strindex		= attributes.Get("relation", data) ? m_strings.get(data) : 0;
		attributes.Get("mask", configuration_condition.m_mask);
		attributes.Get("value", configuration_condition.m_value);
	});
	xml.OnElementBegin({ "mame", "machine", "device" }, [this, &current_device_extensions](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool mandatory;
		info::binaries::device &device = m_devices.emplace_back();
		device.m_type_strindex			= attributes.Get("type", data) ? m_strings.get(data) : 0;
		device.m_tag_strindex			= attributes.Get("tag", data) ? m_strings.get(data) : 0;
		device.m_mandatory				= attributes.Get("mandatory", mandatory) && mandatory ? 1 : 0;
		device.m_instance_name_strindex	= 0;
		device.m_extensions_strindex	= 0;

		current_device_extensions.clear();

		util::last(m_machines).m_devices_count++;
	});
	xml.OnElementBegin({ "mame", "machine", "device", "instance" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		if (attributes.Get("name", data))
			util::last(m_devices).m_instance_name_strindex = m_strings.get(data);
	});
	xml.OnElementBegin({ "mame", "machine", "device", "extension" }, [this, &current_device_extensions](const XmlParser::Attributes &attributes)
	{
		std::string name;
		if (attributes.Get("name", name))
		{
			current_device_extensions.append(name);
			current_device_extensions.append(",");
		}
	});
	xml.OnElementEnd({ "mame", "machine", "device" }, [this, &current_device_extensions](wxString &&)
	{
		if (!current_device_extensions.empty())
			util::last(m_devices).m_extensions_strindex = m_strings.get(current_device_extensions);
	});

	// parse!
	bool success;
	try
	{
		success = xml.Parse(input);
	}
	catch (std::exception &ex)
	{
		error_message = ex.what();
		return false;
	}
	if (!success)
	{
		error_message = xml.ErrorMessage();
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

	// and salt it
	m_salted_header = util::salt(header, info::binaries::salt());

	// success!
	error_message.clear();
	return true;
}


//-------------------------------------------------
//  emit()
//-------------------------------------------------

void info::database_builder::emit(wxOutputStream &output)
{
	output.Write(&m_salted_header, sizeof(m_salted_header));
	output.Write(m_machines.data(), m_machines.size() * sizeof(m_machines[0]));
	output.Write(m_devices.data(), m_devices.size() * sizeof(m_devices[0]));
	output.Write(m_configurations.data(), m_configurations.size() * sizeof(m_configurations[0]));
	output.Write(m_configuration_settings.data(), m_configuration_settings.size() * sizeof(m_configuration_settings[0]));
	output.Write(m_configuration_conditions.data(), m_configuration_conditions.size() * sizeof(m_configuration_conditions[0]));
	output.Write(m_strings.data().data(), m_strings.data().size() * sizeof(m_strings.data()[0]));
}


//-------------------------------------------------
//  string_table ctor
//-------------------------------------------------

info::database_builder::string_table::string_table()
{
	// reserve space based on expected size (see comments above)
	m_data.reserve(2400000);		// 2012056 bytes
	m_map.reserve(100000);			// 91699 entries

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


std::uint32_t info::database_builder::string_table::get(const wxString &s)
{
	return get(std::string(s.ToUTF8()));
}


//-------------------------------------------------
//  string_table::data
//-------------------------------------------------

const std::vector<char> &info::database_builder::string_table::data() const
{
	return m_data;
}
