/***************************************************************************

	info_builder.cpp

	Code to build MAME info DB

***************************************************************************/

#include <wx/wfstream.h>
#include <wx/mstream.h>

#include "info_builder.h"
#include "xmlparser.h"
#include "validity.h"


//**************************************************************************
//  LOCALS
//**************************************************************************

static const util::enum_parser<info::software_list::status_type> s_status_type =
{
	{ "original", info::software_list::status_type::ORIGINAL, },
	{ "compatible", info::software_list::status_type::COMPATIBLE }
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
		machine.m_software_lists_index	= to_uint32(m_software_lists.size());
		machine.m_software_lists_count	= 0;
		machine.m_ram_options_index		= to_uint32(m_ram_options.size());
		machine.m_ram_options_count		= 0;
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
	xml.OnElementBegin({ "mame", "machine", "softwarelist" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::software_list::status_type status;
		info::binaries::software_list &software_list = m_software_lists.emplace_back();
		software_list.m_name_strindex			= attributes.Get("name", data) ? m_strings.get(data) : 0;
		software_list.m_filter_strindex			= attributes.Get("filter", data) ? m_strings.get(data) : 0;
		software_list.m_status					= (uint8_t) (attributes.Get("status", status, s_status_type) ? status : info::software_list::status_type::ORIGINAL);
		util::last(m_machines).m_software_lists_count++;
	});
	xml.OnElementBegin({ "mame", "machine", "ramoption" }, [this](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool b;
		info::binaries::ram_option &ram_option = m_ram_options.emplace_back();
		ram_option.m_name_strindex				= attributes.Get("name", data) ? m_strings.get(data) : 0;
		ram_option.m_is_default					= attributes.Get("default", b) && b;
		ram_option.m_value						= 0;
		util::last(m_machines).m_ram_options_count++;
	});
	xml.OnElementEnd({ "mame", "machine", "ramoption" }, [this](wxString &&content)
	{
		unsigned long val;
		util::last(m_ram_options).m_value = content.ToULong(&val) ? val : 0;
	});

	// parse!
	bool success;
	try
	{
		success = xml.Parse(input);
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
	header.m_software_lists_count			= to_uint32(m_software_lists.size());
	header.m_ram_options_count				= to_uint32(m_ram_options.size());

	// and salt it
	m_salted_header = util::salt(header, info::binaries::salt());

	// success!
	error_message.clear();
	return true;
}


//-------------------------------------------------
//  emit()
//-------------------------------------------------

void info::database_builder::emit(wxOutputStream &output) const
{
	output.Write(&m_salted_header, sizeof(m_salted_header));
	output.Write(m_machines.data(),					m_machines.size()					* sizeof(m_machines[0]));
	output.Write(m_devices.data(),					m_devices.size()					* sizeof(m_devices[0]));
	output.Write(m_configurations.data(),			m_configurations.size()				* sizeof(m_configurations[0]));
	output.Write(m_configuration_settings.data(),	m_configuration_settings.size()		* sizeof(m_configuration_settings[0]));
	output.Write(m_configuration_conditions.data(),	m_configuration_conditions.size()	* sizeof(m_configuration_conditions[0]));
	output.Write(m_software_lists.data(),			m_software_lists.size()				* sizeof(m_software_lists[0]));
	output.Write(m_ram_options.data(),				m_ram_options.size()				* sizeof(m_ram_options[0]));
	output.Write(m_strings.data().data(),			m_strings.data().size()				* sizeof(m_strings.data()[0]));
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


//**************************************************************************
//  VALIDITY CHECKS
//**************************************************************************

//-------------------------------------------------
//  read_sample_listxml
//-------------------------------------------------

static bool read_sample_listxml(wxOutputStream &output)
{
#ifdef __WINDOWS__
	// load the resource
	const void *buffer;
	size_t buffer_length;
	bool success = wxLoadUserResource(&buffer, &buffer_length, wxT("testasset_listxml"));
	assert(success);
	assert(buffer);
	assert(buffer_length > 0);
	wxMemoryInputStream input(buffer, buffer_length);

	// process the sample -listxml output
	info::database_builder builder;
	wxString error_message;
	success = builder.process_xml(input, error_message);
	assert(success);
	assert(error_message.empty());
	(void)success;

	// and emit the results into the memory stream
	builder.emit(output);
	return true;
#else
	// gracefully fail if not on Windows
	return false;
#endif
}


//-------------------------------------------------
//  test
//-------------------------------------------------

static void test()
{
	// build the sample database
	wxMemoryOutputStream mem_output_stream;
	if (!read_sample_listxml(mem_output_stream))
		return;

	// and process it, validating we've done so successfully
	wxMemoryInputStream mem_input_stream(mem_output_stream);
	info::database db;
	bool db_changed = false;
	db.set_on_changed([&db_changed]() { db_changed = true; });
	bool success = db.load(mem_input_stream);
	assert(success);
	assert(db_changed);
	(void)success;

	// spelunk through the resulting db
	int setting_count = 0, software_list_count = 0, ram_option_count = 0;
	for (info::machine machine : db.machines())
	{
		// basic machine properties
		const wxString &name = machine.name();
		const wxString &description = machine.description();
		assert(!name.empty());
		assert(!description.empty());
		(void)name;
		(void)description;

		for (info::device dev : machine.devices())
		{
			const wxString &type = dev.type();
			const wxString &tag = dev.tag();
			const wxString &instance_name = dev.instance_name();
			const wxString &extensions = dev.extensions();
			assert(!type.empty());
			assert(!tag.empty());
			assert(!instance_name.empty());
			assert(!extensions.empty());
			(void)type;
			(void)tag;
			(void)instance_name;
			(void)extensions;
		}

		for (info::configuration cfg : machine.configurations())
		{
			for (info::configuration_setting setting : cfg.settings())
				setting_count++;
		}

		for (info::software_list swlist : machine.software_lists())
			software_list_count++;

		for (info::ram_option ramopt : machine.ram_options())
			ram_option_count++;
	}
	assert(setting_count > 0);
	assert(software_list_count > 0);
	assert(ram_option_count > 0);
}


//-------------------------------------------------
//  validity_checks
//-------------------------------------------------

static validity_check validity_checks[] =
{
	test,
};
