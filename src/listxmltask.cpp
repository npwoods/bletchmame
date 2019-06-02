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
	protected:
		virtual std::vector<wxString> GetArguments(const Preferences &) const
		{
			return { "-listxml", "-nodtd", "-lightxml" };
		}

		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;

	private:
		ListXmlResult InternalProcess(wxInputStream &input);
	};
}

//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_LIST_XML_RESULT, PayloadEvent<ListXmlResult>);


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
	XmlParser xml;
    ListXmlResult result;

	// as of 2-Jun-2019, MAME 0.210 has 35947 machines; reserve space in the vector with clearance for the future
	result.m_machines.reserve(40000);

	std::vector<Machine>::iterator current_machine;

	xml.OnElement({ "mame" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_build = attributes["build"];
	});
	xml.OnElement({ "mame", "machine" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_machines.emplace_back();
		current_machine = result.m_machines.end() - 1;
		current_machine->m_name			= attributes["name"];
		current_machine->m_sourcefile	= attributes["sourcefile"];
		current_machine->m_clone_of		= attributes["cloneof"];
		current_machine->m_rom_of		= attributes["romof"];
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

	result.m_success = xml.Parse(input);

	// we're done processing!  shrink the vector and move on
	result.m_machines.shrink_to_fit();
	return result;
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task()
{
	return std::make_unique<ListXmlTask>();
}

