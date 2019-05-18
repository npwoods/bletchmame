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

void MameClient::Launch(std::unique_ptr<Task> &&task, int)
{
    // Sanity check; don't do anything if we already have a task
    if (m_task)
    {
        throw false;
    }

	// build a command to launch the MAME slave
    std::string launch_command = m_site.GetMameCommand().ToStdString() + " " + task->Arguments();

	// set up the wxProcess, and work around the odd lifecycle of this wxWidgetism
	auto process = std::shared_ptr<MyProcess>(new MyProcess());
	m_process_for_main_thread = process;
	m_process_for_task_thread = process;
	process->Redirect();
	process->SetProcess(process);

    long process_id = ::wxExecute(launch_command, wxEXEC_ASYNC, m_process_for_main_thread.get());
    if (process_id == 0)
    {
        throw false;
    }

    s_job.AddProcess(process_id);

    m_task = std::move(task);
    m_thread = std::thread([this]
    {
		// we should really have logging, but wxWidgets handles logging oddly from child threads
		wxLog::EnableLogging(false);

		// invoke the task's process method
        m_task->Process(*m_process_for_task_thread, m_site.EventHandler());

		// we're done with our handle to the process
		m_process_for_task_thread.reset();
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
	if (m_process_for_main_thread)
		m_process_for_main_thread.reset();

	// the following should never happen, as the act of joining the thread should cause
	// this to be cleared, but so be it
	if (m_process_for_task_thread)
		m_process_for_task_thread.reset();
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void MameClient::Abort()
{
	if (m_task)
		m_task->Abort();
	if (m_process_for_main_thread)
		wxKill(m_process_for_main_thread->GetPid(), wxSIGKILL);
}
