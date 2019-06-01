/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

#include <wx/xml/xml.h>

#include "listxmltask.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class BlockingInputStream : public wxInputStream
	{
	public:
		BlockingInputStream(wxInputStream &input)
			: m_input(input)
		{
		}

	protected:
		virtual size_t OnSysRead(void *buffer, size_t bufsize)
		{
			return InternalOnSysRead(static_cast<uint8_t *>(buffer), bufsize);
		}

	private:
		wxInputStream & m_input;

		size_t InternalOnSysRead(uint8_t *buffer, size_t bufsize)
		{
			bool done = false;
			size_t position = 0;

			while (!done && position < bufsize)
			{
				m_input.Read(&buffer[position], bufsize - position);
				position += m_input.LastRead();
				done = m_input.LastRead() == 0;
			}

			return position;
		}

	};

	class ListXmlTask : public Task
	{
	public:
		virtual std::vector<wxString> GetArguments(const Preferences &) const
		{
			return { "-listxmllight", "-nodtd" };
		}

		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;

	private:
		ListXmlResult InternalProcess(wxInputStream &input);
		Machine MachineFromXmlNode(wxXmlNode &node);

		bool LoadXml(wxXmlDocument &xml, wxInputStream &input)
		{
			// wxXmlDocument::Load() seems to interpret a stream that returns less than the expected amount of
			// bytes as a sign of being done.  We're using a wxBufferedInputStream to get around this behavior
			BlockingInputStream blocking_stream(input);
			return xml.Load(blocking_stream);
		}
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
    ListXmlResult result;

    wxXmlDocument xml;
    if (!LoadXml(xml, input))
    {
        result.m_success = false;
        return result;
    }

    wxXmlNode &root(*xml.GetRoot());
    result.m_build = root.GetAttribute("build");

    for (wxXmlNode *child = root.GetChildren(); child; child = child->GetNext())
    {
        if (child->GetName() == "machine")
        {
            wxString is_device = child->GetAttribute("isdevice");
            if (is_device == "" || child->GetAttribute("runnable") == "yes")
            {
                Machine machine = MachineFromXmlNode(*child);
                result.m_machines.push_back(std::move(machine));
            }
        }
    }

    result.m_success = true;
    return result;
}


//-------------------------------------------------
//  MachineFromXmlNode
//-------------------------------------------------

Machine ListXmlTask::MachineFromXmlNode(wxXmlNode &node)
{
    Machine machine;

    machine.m_name = node.GetAttribute("name");
    machine.m_sourcefile = node.GetAttribute("sourcefile");
    machine.m_clone_of = node.GetAttribute("cloneof");
    machine.m_rom_of = node.GetAttribute("romof");

    for (wxXmlNode *child = node.GetChildren(); child; child = child->GetNext())
    {
        if (child->GetName() == "description")
            machine.m_description = child->GetNodeContent();
        else if (child->GetName() == "year")
            machine.m_year = child->GetNodeContent();
        else if (child->GetName() == "manufacturer")
            machine.m_manfacturer = child->GetNodeContent();
    }

    return machine;
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task()
{
	return std::make_unique<ListXmlTask>();
}

