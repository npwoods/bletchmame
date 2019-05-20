/***************************************************************************

    client.cpp

    MAME Executable Client

***************************************************************************/

#include "client.h"


// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/sstream.h>
#include <iostream>
#include <thread>

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "client.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
    class MyProcess : public wxProcess
    {
    public:
		void SetProcess(std::shared_ptr<wxProcess> &&process)
        {
			m_process = std::move(process);
        }

        virtual void OnTerminate(int pid, int status) override
        {
			wxLogStatus("Slave process terminated; pid=%d status=%d", pid, status);
        }

	private:
		std::shared_ptr<wxProcess> m_process;
    };
}


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

Job MameClient::s_job;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameClient::MameClient(IMameClientSite &site)
    : m_site(site)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MameClient::~MameClient()
{
	Abort();
    Reset();
}



//-------------------------------------------------
//  Launch
//-------------------------------------------------

void MameClient::Launch(std::unique_ptr<Task> &&task)
{
	// Sanity check; don't do anything if we already have a task
	if (m_task)
	{
		throw false;
	}

	// build a command to launch the MAME slave
	const wxString &mame_path(m_site.GetMamePath());
	const wxString &mame_extra_arguments(m_site.GetMameExtraArguments());
	wxString launch_command = (mame_path.Contains(" ") ? "\"" + mame_path + "\"" : mame_path)
		+ " " + task->Arguments()
		+ (mame_extra_arguments.IsEmpty() ? wxEmptyString : " " + mame_extra_arguments);

	// set up the wxProcess, and work around the odd lifecycle of this wxWidgetism
	auto process = std::make_shared<MyProcess>();
	m_process = process;
	process->Redirect();
	process->SetProcess(process);

	// launch the process
	long process_id = ::wxExecute(launch_command, wxEXEC_ASYNC, m_process.get());
	if (process_id == 0)
	{
		// TODO - better error handling, especially when we're not pointed at the proper executable
		throw false;
	}

	s_job.AddProcess(process_id);

	m_task = std::move(task);
	m_thread = std::thread([process, this]()
	{
		// we should really have logging, but wxWidgets handles logging oddly from child threads
		wxLog::EnableLogging(false);

		// invoke the task's process method
		m_task->Process(*process, m_site.EventHandler());
	});
}


//-------------------------------------------------
//  Reset
//-------------------------------------------------

void MameClient::Reset()
{
    if (m_thread.joinable())
        m_thread.join();
    if (m_task)
        m_task.reset();
	if (m_process)
		m_process.reset();
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void MameClient::Abort()
{
	if (m_task)
		m_task->Abort();
	if (m_process)
		wxKill(m_process->GetPid(), wxSIGKILL);
}
