/***************************************************************************

    versiontask.cpp

    Task for invoking '-version'

***************************************************************************/

#include <wx/txtstrm.h>

#include "versiontask.h"
#include "task.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> VersionTask
	class VersionTask : public Task
	{
	protected:
		virtual std::vector<wxString> GetArguments(const Preferences &) const;
		virtual void Process(wxProcess &process, wxEvtHandler &handler) override;
		virtual void Abort() override;
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

wxDEFINE_EVENT(EVT_VERSION_RESULT, PayloadEvent<VersionResult>);


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

std::vector<wxString> VersionTask::GetArguments(const Preferences &) const
{
	return { "-version" };
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void VersionTask::Abort()
{
	// do nothing
}


//-------------------------------------------------
//  Process
//-------------------------------------------------

void VersionTask::Process(wxProcess &process, wxEvtHandler &handler)
{
	// read text
	wxTextInputStream input(*process.GetInputStream());

	// get the version
	VersionResult result;
	result.m_version = input.ReadLine();

	// and send the results back
	util::QueueEvent(handler, EVT_VERSION_RESULT, wxID_ANY, std::move(result));
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_version_task()
{
	return std::make_unique<VersionTask>();
}

