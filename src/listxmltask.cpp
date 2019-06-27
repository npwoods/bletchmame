/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

#include <wx/wfstream.h>
#include <unordered_map>

#include "listxmltask.h"
#include "xmlparser.h"
#include "utility.h"
#include "info.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ListXmlTask : public Task
	{
	public:
		ListXmlTask(std::unique_ptr<wxOutputStream> &&output);

	protected:
		virtual std::vector<wxString> GetArguments(const Preferences &) const;
		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;

	private:
		std::unique_ptr<wxOutputStream>	m_output;

		ListXmlResult InternalProcess(wxInputStream &input);
	};

	class string_table
	{
	public:
		string_table();
		std::uint32_t get(const std::string &string);
		std::uint32_t get(const wxString &string);
		const std::vector<char> &data() const;

	private:
		std::vector<char>								m_data;
		std::unordered_map<std::string, std::uint32_t>	m_map;
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


//-------------------------------------------------
//  to_uint32
//-------------------------------------------------

template<typename T>
static std::uint32_t to_uint32(T &&value)
{
	std::uint32_t new_value = static_cast<std::uint32_t>(value);
	if (new_value != value)
		throw false;
	return new_value;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ListXmlTask::ListXmlTask(std::unique_ptr<wxOutputStream> &&output)
	: m_output(std::move(output))
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> ListXmlTask::GetArguments(const Preferences &) const
{
	return { "-listxml", "-nodtd" };
}


//-------------------------------------------------
//  Process
//-------------------------------------------------

void ListXmlTask::Process(wxProcess &process, wxEvtHandler &handler)
{
	ListXmlResult result = InternalProcess(*process.GetInputStream());
	util::QueueEvent(handler, EVT_LIST_XML_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  InternalProcess
//-------------------------------------------------

ListXmlResult ListXmlTask::InternalProcess(wxInputStream &input)
{
	ListXmlResult result;

	// prepare data
	info::binaries::header header = { 0, };
	std::vector<info::binaries::machine> machines;
	std::vector<info::binaries::device> devices;
	std::vector<info::binaries::configuration> configurations;
	std::vector<info::binaries::configuration_condition> configuration_conditions;
	std::vector<info::binaries::configuration_setting> configuration_settings;
	string_table strings;

	// reserve space based on what we know about MAME as of 24-Jun-2019 (MAME 0.211)
	machines.reserve(45000);				// 40455 machines
	devices.reserve(9000);					// 8396 devices
	configurations.reserve(32000);			// 29662 configurations
	configuration_conditions.reserve(250);	// 216 conditions
	configuration_settings.reserve(350000);	// 312460 settings

	// magic/version
	header.m_magic = info::binaries::MAGIC;
	header.m_version = info::binaries::VERSION;

	// parse the -listxml output
	XmlParser xml;
	std::string current_device_extensions;
	xml.OnElement({ "mame" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string build;
		if (attributes.Get("build", build))
			header.m_build_strindex = strings.get(build);
	});
	xml.OnElement({ "mame", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::machine machine = { 0, };
		if (attributes.Get("name", data))
			machine.m_name_strindex = strings.get(data);
		if (attributes.Get("sourcefile", data))
			machine.m_sourcefile_strindex = strings.get(data);
		if (attributes.Get("cloneof", data))
			machine.m_clone_of_strindex = strings.get(data);
		if (attributes.Get("romof", data))
			machine.m_rom_of_strindex = strings.get(data);
		machine.m_configurations_index = to_uint32(configurations.size());
		machine.m_devices_index = to_uint32(devices.size());
		machines.push_back(std::move(machine));
	});
	xml.OnElement({ "mame", "machine", "description" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_description_strindex = strings.get(content);
	});
	xml.OnElement({ "mame", "machine", "year" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_year_strindex = strings.get(content);
	});
	xml.OnElement({ "mame", "machine", "manufacturer" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_manufacturer_strindex = strings.get(content);
	});
	xml.OnElement({ "mame", "machine", "configuration" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration configuration = { 0, };
		if (attributes.Get("name", data))
			configuration.m_name_strindex = strings.get(data);
		if (attributes.Get("tag", data))
			configuration.m_tag_strindex = strings.get(data);
		configurations.push_back(std::move(configuration));
	
		auto iter = machines.end() - 1;
		iter->m_configurations_count++;
	});
	xml.OnElement({ "mame", "machine", "configuration", "confsetting" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_setting configuration_setting = { 0, };
		if (attributes.Get("name", data))
			configuration_setting.m_name_strindex = strings.get(data);
		attributes.Get("value", configuration_setting.m_value);
		configuration_settings.push_back(std::move(configuration_setting));
	});
	xml.OnElement({ "mame", "machine", "configuration", "confsetting", "condition" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_condition configuration_condition = { 0, };
		if (attributes.Get("tag", data))
			configuration_condition.m_tag_strindex = strings.get(data);
		if (attributes.Get("relation", data))
			configuration_condition.m_relation_strindex = strings.get(data);
		attributes.Get("mask", configuration_condition.m_mask);
		attributes.Get("value", configuration_condition.m_value);
		configuration_conditions.push_back(std::move(configuration_condition));
	});
	xml.OnElement({ "mame", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool mandatory;
		info::binaries::device device = { 0, };
		if (attributes.Get("type", data))
			device.m_type_strindex = strings.get(data);
		if (attributes.Get("tag", data))
			device.m_tag_strindex = strings.get(data);
		attributes.Get("mandatory", mandatory);
		device.m_mandatory = mandatory ? 1 : 0;
		devices.push_back(std::move(device));

		current_device_extensions.clear();

		auto iter = machines.end() - 1;
		iter->m_devices_count++;
	});
	xml.OnElement({ "mame", "machine", "device", "instance" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		auto iter = devices.end() - 1;
		if (attributes.Get("name", data))
			iter->m_instance_name_strindex = strings.get(data);
	});
	xml.OnElement({ "mame", "machine", "device", "extension" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string name;
		if (attributes.Get("name", name))
		{
			current_device_extensions.append(name);
			current_device_extensions.append(",");
		}
	});
	xml.OnElement({ "mame", "machine", "device" }, [&](wxString &&)
	{
		if (!current_device_extensions.empty())
		{
			auto iter = devices.end() - 1;
			iter->m_extensions_strindex = strings.get(current_device_extensions);
		}
	});
	xml.Parse(input);

	// record any error messages
	if (!result.m_success)
		result.m_error_message = xml.ErrorMessage();

	// finalize the header
	header.m_machines_count					= to_uint32(machines.size());
	header.m_devices_count					= to_uint32(devices.size());
	header.m_configurations_count			= to_uint32(configurations.size());
	header.m_configuration_settings_count	= to_uint32(configuration_settings.size());
	header.m_configuration_conditions_count	= to_uint32(configuration_conditions.size()); 
	
	// emit the data
	m_output->Write(&header,							sizeof(header));
	m_output->Write(machines.data(),					machines.size()					* sizeof(machines[0]));
	m_output->Write(devices.data(),						devices.size()					* sizeof(devices[0]));
	m_output->Write(configurations.data(),				configurations.size()			* sizeof(configurations[0]));
	m_output->Write(configuration_settings.data(),		configuration_settings.size()	* sizeof(configuration_settings[0]));
	m_output->Write(configuration_conditions.data(),	configuration_conditions.size()	* sizeof(configuration_conditions[0]));

	// note that on the string table, we know the last character is \0 so we can drop it
	m_output->Write(strings.data().data(),				(strings.data().size() - 1)		* sizeof(strings.data()[0]));

	// close out the file and return
	m_output.reset();
	return result;
}


//-------------------------------------------------
//  string_table ctor
//-------------------------------------------------

string_table::string_table()
{
	// reserve space based on expected size (see comments above)
	m_data.reserve(2400000);		// 2012056 bytes
	m_map.reserve(100000);			// 91699 entries

	// special case; prime empty string to be #0
	get(std::string());
}


//-------------------------------------------------
//  string_table::get
//-------------------------------------------------

std::uint32_t string_table::get(const std::string &s)
{
	auto iter = m_map.find(s);
	if (iter != m_map.end())
		return iter->second;

	// append the string to m_data
	std::uint32_t result = to_uint32(m_data.size());
	for (size_t i = 0; i < s.size(); i++)
		m_data.push_back(s[i]);
	m_data.push_back('\0');

	// and to m_map
	m_map[s] = result;

	// and return
	return result;
}


std::uint32_t string_table::get(const wxString &s)
{
	return get(std::string(s.ToUTF8()));
}


//-------------------------------------------------
//  string_table::data
//-------------------------------------------------

const std::vector<char> &string_table::data() const
{
	return m_data;
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task(wxString &&dest)
{
	std::unique_ptr<wxOutputStream> output = std::make_unique<wxFileOutputStream>(dest);
	return std::make_unique<ListXmlTask>(std::move(output));
}
