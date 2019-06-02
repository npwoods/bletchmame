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
#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
    class ClientProcess : public wxProcess
    {
    public:
		ClientProcess(std::function<void(Task::emu_error status)> func)
			: m_on_terminate_func(std::move(func))
		{
		}

		void SetProcess(std::shared_ptr<wxProcess> &&process)
        {
			m_process = std::move(process);
        }

        virtual void OnTerminate(int pid, int status) override
        {
			wxLogStatus("Slave process terminated; pid=%d status=%d", pid, status);
			m_on_terminate_func(static_cast<Task::emu_error>(status));
        }

	private:
		std::function<void(Task::emu_error status)>	m_on_terminate_func;
		std::shared_ptr<wxProcess>		m_process;
    };
}


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

Job MameClient::s_job;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameClient::MameClient(wxEvtHandler &event_handler, const Preferences &prefs)
    : m_event_handler(event_handler)
	, m_prefs(prefs)
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

void MameClient::Launch(Task::ptr &&task)
{
	// Sanity check; don't do anything if we already have a task
	if (m_task)
	{
		throw false;
	}

	// build the command line
	wxString launch_command = util::build_command_line(
		m_prefs.GetPath(Preferences::path_type::emu_exectuable),
		task->GetArguments(m_prefs));

	// slap on any extra arguments
	const wxString &extra_arguments(m_prefs.GetMameExtraArguments());
	if (!extra_arguments.IsEmpty())
		launch_command += " " + extra_arguments;

	// set up the wxProcess, and work around the odd lifecycle of this wxWidgetism
	auto process = std::make_shared<ClientProcess>([task](Task::emu_error status) { task->OnTerminate(status); });
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
		m_task->Process(*process, m_event_handler);
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
