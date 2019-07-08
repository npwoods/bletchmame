/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

#include <wx/wfstream.h>
#include <unordered_map>
#include <exception>

#include "listxmltask.h"
#include "xmlparser.h"
#include "utility.h"
#include "info.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> ListXmlTask
	class ListXmlTask : public Task
	{
	public:
		ListXmlTask(wxString &&output_filename);

	protected:
		virtual std::vector<wxString> GetArguments(const Preferences &) const;
		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;
		virtual void Abort() override;

	private:
		wxString		m_output_filename;
		volatile bool	m_aborted;

		void InternalProcess(wxInputStream &input);
	};

	// ======================> string_table
	class string_table
	{
	public:
		string_table();
		std::uint32_t get(const std::string &string);
		std::uint32_t get(const wxString &string);
		const std::vector<char> &data() const;
		
		template<typename T> void embed_value(T value)
		{
			const std::uint8_t *bytes = (const std::uint8_t *) &value;
			for (int i = 0; i < sizeof(value); i++)
				m_data.push_back(bytes[i]);
		}

	private:
		std::vector<char>								m_data;
		std::unordered_map<std::string, std::uint32_t>	m_map;
	};

	// ======================> list_xml_exception
	class list_xml_exception : public std::exception
	{
	public:
		list_xml_exception(ListXmlResult::status status, wxString &&message = wxString())
			: m_status(status)
			, m_message(message)
		{
		}

		ListXmlResult::status	m_status;
		wxString				m_message;
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
		throw list_xml_exception(ListXmlResult::status::ERROR, "Array size cannot fit in 32 bits");
	return new_value;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ListXmlTask::ListXmlTask(wxString &&output_filename)
	: m_output_filename(std::move(output_filename))
	, m_aborted(false)
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
//  Abort
//-------------------------------------------------

void ListXmlTask::Abort()
{
	m_aborted = true;
}


//-------------------------------------------------
//  Process
//-------------------------------------------------

void ListXmlTask::Process(wxProcess &process, wxEvtHandler &handler)
{
	ListXmlResult result;
	try
	{
		// process
		InternalProcess(*process.GetInputStream());

		// we've succeeded!
		result.m_status = ListXmlResult::status::SUCCESS;
	}
	catch (list_xml_exception &ex)
	{
		// an exception has occurred
		result.m_status = ex.m_status;
		result.m_error_message = std::move(ex.m_message);
	}

	// regardless of what happened, notify the main thread
	util::QueueEvent(handler, EVT_LIST_XML_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  InternalProcess
//-------------------------------------------------

void ListXmlTask::InternalProcess(wxInputStream &input)
{
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
	header.m_magic = info::binaries::MAGIC_HEADER;
	header.m_version = info::binaries::VERSION;

	// parse the -listxml output
	XmlParser xml;
	std::string current_device_extensions;
	xml.OnElementBegin({ "mame" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string build;
		if (attributes.Get("build", build))
			header.m_build_strindex = strings.get(build);
	});
	xml.OnElementBegin({ "mame", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		bool runnable;
		if (attributes.Get("runnable", runnable) && !runnable)
			return XmlParser::element_result::SKIP;

		std::string data;
		info::binaries::machine &machine = machines.emplace_back();
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
		return XmlParser::element_result::OK;
	});
	xml.OnElementEnd({ "mame", "machine", "description" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_description_strindex = strings.get(content);
	});
	xml.OnElementEnd({ "mame", "machine", "year" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_year_strindex = strings.get(content);
	});
	xml.OnElementEnd({ "mame", "machine", "manufacturer" }, [&](wxString &&content)
	{
		auto iter = machines.end() - 1;
		iter->m_manufacturer_strindex = strings.get(content);
	});
	xml.OnElementBegin({ "mame", "machine", "configuration" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration &configuration = configurations.emplace_back();
		if (attributes.Get("name", data))
			configuration.m_name_strindex = strings.get(data);
		if (attributes.Get("tag", data))
			configuration.m_tag_strindex = strings.get(data);
		attributes.Get("mask", configuration.m_mask);
		configuration.m_configuration_settings_index = to_uint32(configuration_settings.size());
	
		util::last(machines).m_configurations_count++;
	});
	xml.OnElementBegin({ "mame", "machine", "configuration", "confsetting" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_setting &configuration_setting = configuration_settings.emplace_back();
		if (attributes.Get("name", data))
			configuration_setting.m_name_strindex = strings.get(data);
		attributes.Get("value", configuration_setting.m_value);

		util::last(configurations).m_configuration_settings_count++;
	});
	xml.OnElementBegin({ "mame", "machine", "configuration", "confsetting", "condition" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		info::binaries::configuration_condition &configuration_condition = configuration_conditions.emplace_back();
		if (attributes.Get("tag", data))
			configuration_condition.m_tag_strindex = strings.get(data);
		if (attributes.Get("relation", data))
			configuration_condition.m_relation_strindex = strings.get(data);
		attributes.Get("mask", configuration_condition.m_mask);
		attributes.Get("value", configuration_condition.m_value);
	});
	xml.OnElementBegin({ "mame", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		bool mandatory;
		info::binaries::device &device = devices.emplace_back();
		if (attributes.Get("type", data))
			device.m_type_strindex = strings.get(data);
		if (attributes.Get("tag", data))
			device.m_tag_strindex = strings.get(data);
		attributes.Get("mandatory", mandatory);
		device.m_mandatory = mandatory ? 1 : 0;

		current_device_extensions.clear();

		util::last(machines).m_devices_count++;
	});
	xml.OnElementBegin({ "mame", "machine", "device", "instance" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string data;
		if (attributes.Get("name", data))
			util::last(devices).m_instance_name_strindex = strings.get(data);
	});
	xml.OnElementBegin({ "mame", "machine", "device", "extension" }, [&](const XmlParser::Attributes &attributes)
	{
		std::string name;
		if (attributes.Get("name", name))
		{
			current_device_extensions.append(name);
			current_device_extensions.append(",");
		}
	});
	xml.OnElementEnd({ "mame", "machine", "device" }, [&](wxString &&)
	{
		if (!current_device_extensions.empty())
			util::last(devices).m_extensions_strindex = strings.get(current_device_extensions);
	});

	// parse!
	bool success = xml.Parse(input);

	// before we check to see if there is a parsing error, check for an abort - under which
	// scenario a parsing error is expected
	if (m_aborted)
		throw list_xml_exception(ListXmlResult::status::ABORTED);

	// now check for a parse error (which should be very unlikely)
	if (!success)
		throw list_xml_exception(ListXmlResult::status::ERROR, wxString(wxT("Error parsing XML from MAME -listxml: ")) + xml.ErrorMessage());

	// final magic bytes on string table
	strings.embed_value(info::binaries::MAGIC_STRINGTABLE_END);

	// finalize the header
	header.m_machines_count					= to_uint32(machines.size());
	header.m_devices_count					= to_uint32(devices.size());
	header.m_configurations_count			= to_uint32(configurations.size());
	header.m_configuration_settings_count	= to_uint32(configuration_settings.size());
	header.m_configuration_conditions_count	= to_uint32(configuration_conditions.size()); 
	
	// we finally have all of the info accumulated; now we can get to business with writing
	// to the actual file
	wxFileOutputStream output(m_output_filename);
	if (!output.IsOk())
		throw list_xml_exception(ListXmlResult::status::ERROR, wxString(wxT("Could not open file: ")) + m_output_filename);

	// emit the data
	output.Write(&header,							sizeof(header));
	output.Write(machines.data(),					machines.size()					* sizeof(machines[0]));
	output.Write(devices.data(),					devices.size()					* sizeof(devices[0]));
	output.Write(configurations.data(),				configurations.size()			* sizeof(configurations[0]));
	output.Write(configuration_settings.data(),		configuration_settings.size()	* sizeof(configuration_settings[0]));
	output.Write(configuration_conditions.data(),	configuration_conditions.size()	* sizeof(configuration_conditions[0]));
	output.Write(strings.data().data(),				strings.data().size()			* sizeof(strings.data()[0]));
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

	// embed the initial magic bytes
	embed_value(info::binaries::MAGIC_STRINGTABLE_BEGIN);
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
	return std::make_unique<ListXmlTask>(std::move(dest));
}
