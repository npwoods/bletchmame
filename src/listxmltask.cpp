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
#include "info_builder.h"


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
	info::database_builder builder;

	// first process the XML
	wxString error_message;
	bool success = builder.process_xml(input, error_message);

	// before we check to see if there is a parsing error, check for an abort - under which
	// scenario a parsing error is expected
	if (m_aborted)
		throw list_xml_exception(ListXmlResult::status::ABORTED);

	// now check for a parse error (which should be very unlikely)
	if (!success)
		throw list_xml_exception(ListXmlResult::status::ERROR, wxString(wxT("Error parsing XML from MAME -listxml: ")) + error_message);

	// we finally have all of the info accumulated; now we can get to business with writing
	// to the actual file
	wxFileOutputStream output(m_output_filename);
	if (!output.IsOk())
		throw list_xml_exception(ListXmlResult::status::ERROR, wxString(wxT("Could not open file: ")) + m_output_filename);

	// emit the data
	builder.emit(output);
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task(wxString &&dest)
{
	return std::make_unique<ListXmlTask>(std::move(dest));
}
