/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

#include "listxmltask.h"
#include "xmlparser.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ListXmlTask : public Task
	{
	public:
		ListXmlTask(bool light, wxString &&target);

	protected:
		virtual std::vector<wxString> GetArguments(const Preferences &) const;
		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;

	private:
		bool		m_light;
		wxString	m_target;

		ListXmlResult InternalProcess(wxInputStream &input);
	};
}

//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ListXmlTask::ListXmlTask(bool light, wxString &&target)
	: m_light(light)
	, m_target(std::move(target))
{
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> ListXmlTask::GetArguments(const Preferences &) const
{
	std::vector<wxString> results = { "-listxml", "-nodtd" };
	if (m_light)
		results.push_back("-lightxml");
	if (!m_target.IsEmpty())
		results.push_back(m_target);
	return results;
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
	// intial results setup
	ListXmlResult result;
	result.m_success = false;
	result.m_target = m_target;

	// as of 2-Jun-2019, MAME 0.210 has 35947 machines; reserve space in the vector with clearance for the future
	if (m_target.IsEmpty())
		result.m_machines.reserve(40000);

	// parse the -listxml output
	XmlParser xml;
	std::vector<Machine>::iterator					current_machine;
	std::vector<Configuration>::iterator			current_configuration;
	std::vector<ConfigurationSetting>::iterator		current_setting;
	std::vector<ConfigurationCondition>::iterator	current_condition;
	std::vector<Device>::iterator					current_device;
	xml.OnElement({ "mame" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_build = attributes["build"];
	});
	xml.OnElement({ "mame", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_machines.emplace_back();
		current_machine = result.m_machines.end() - 1;
		current_machine->m_light		= m_light;
		current_machine->m_name			= attributes["name"];
		current_machine->m_sourcefile	= attributes["sourcefile"];
		current_machine->m_clone_of		= attributes["cloneof"];
		current_machine->m_rom_of		= attributes["romof"];
	});
	xml.OnElement({ "mame", "machine" }, [&](wxString &&)
	{
		current_machine->m_devices.shrink_to_fit();
	});
	xml.OnElement({ "mame", "machine", "description" }, [&](wxString &&content)
	{
		current_machine->m_description = std::move(content);
	});
	xml.OnElement({ "mame", "machine", "year" }, [&](wxString &&content)
	{
		current_machine->m_year = std::move(content);
	});
	xml.OnElement({ "mame", "machine", "manufacturer" }, [&](wxString &&content)
	{
		current_machine->m_manfacturer = std::move(content);
	});
	xml.OnElement({ "mame", "machine", "configuration" }, [&](const XmlParser::Attributes &attributes)
	{
		current_machine->m_configurations.emplace_back();
		current_configuration = current_machine->m_configurations.end() - 1;
		current_configuration->m_name	= attributes["name"];
		current_configuration->m_tag	= attributes["tag"];
	});
	xml.OnElement({ "mame", "machine", "configuration", "confsetting" }, [&](const XmlParser::Attributes &attributes)
	{
		current_configuration->m_settings.emplace_back();
		current_setting = current_configuration->m_settings.end() - 1;
		attributes.Get("name",	current_setting->m_name);
		attributes.Get("value",	current_setting->m_value);
	});
	xml.OnElement({ "mame", "machine", "configuration", "confsetting", "condition" }, [&](const XmlParser::Attributes &attributes)
	{
		current_setting->m_conditions.emplace_back();
		current_condition = current_setting->m_conditions.end() - 1;
		attributes.Get("tag",		current_condition->m_tag);
		attributes.Get("mask",		current_condition->m_mask);
		attributes.Get("relation",	current_condition->m_relation);
		attributes.Get("value",		current_condition->m_value);
	});
	xml.OnElement({ "mame", "machine", "device" }, [&](const XmlParser::Attributes &attributes)
	{
		current_machine->m_devices.emplace_back();
		current_device = current_machine->m_devices.end() - 1;
		attributes.Get("type",	current_device->m_type);
		attributes.Get("tag",	current_device->m_tag);
	});
	xml.OnElement({ "mame", "machine", "device", "instance" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("name",	current_device->m_instance_name);
	});
	xml.OnElement({ "mame", "machine", "device", "extension" }, [&](const XmlParser::Attributes &attributes)
	{
		current_device->m_extensions.push_back(attributes["name"]);
	});
	result.m_success = xml.Parse(input);

	// we're done processing!  shrink the vector and move on
	result.m_machines.shrink_to_fit();
	return result;
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task(bool light, const wxString &target)
{
	return std::make_unique<ListXmlTask>(light, wxString(target));
}


