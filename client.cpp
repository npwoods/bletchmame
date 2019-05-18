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
    class ProcessWithTerminateCallback : public wxProcess
    {
    public:
        ProcessWithTerminateCallback(std::function<void(int pid, int status)> &&func)
        {
            m_func = std::move(func);
        }

        virtual void OnTerminate(int pid, int status) override
        {
            m_func(pid, status);
        }

    private:
        std::function<void(int pid, int status)> m_func;
    };
}


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

Job MameClient::s_job;


MameClient::MameClient(IMameClientSite &site)
    : m_site(site)
    , m_process_id(0)
{
}


MameClient::~MameClient()
{
    Reset();
}


void MameClient::Launch(std::unique_ptr<Task> &&task, int)
{
    // Sanity check; don't do anything if we already have a task
    if (m_task)
    {
        throw false;
    }

    std::string launch_command = m_site.GetMameCommand().ToStdString() + " " + task->Arguments();

    m_process = std::unique_ptr<wxProcess>(new ProcessWithTerminateCallback([&](int pid, int status) { this->OnTerminate(pid, status); }));
    m_process->Redirect();

    m_process_id = ::wxExecute(launch_command, wxEXEC_ASYNC, m_process.get());
    if (m_process_id == 0)
    {
        throw false;
    }

    s_job.AddProcess(m_process_id);

    m_task = std::move(task);
    m_thread = std::thread([this]
    {
        m_task->Process(*m_process, m_site.EventHandler());
    });
}


void MameClient::Reset()
{
    if (m_thread.joinable())
        m_thread.join();
    if (m_task)
        m_task.reset();
}


void MameClient::OnTerminate(int, int)
{
    m_process.reset();
}
