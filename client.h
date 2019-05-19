/***************************************************************************

    client.h

    Client for invoking MAME for various tasks

***************************************************************************/

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"
#include <wx/process.h>
#include <wx/txtstrm.h>
#include <wx/msgqueue.h>
#include <wx/xml/xml.h>
#include <iostream>
#include <thread>

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWidgets headers)
#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "task.h"
#include "job.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class IMameClientSite
{
public:
    virtual wxEvtHandler &EventHandler() = 0;
    virtual const wxString &GetMameCommand() = 0;
};


class MameClient
{
public:
    MameClient(IMameClientSite &site);
    ~MameClient();

    void Launch(std::unique_ptr<Task> &&task);
    void Reset();

    template<class T> T *GetCurrentTask()
    {
        return dynamic_cast<T*>(m_task.get());
    }
	template<class T> const T *GetCurrentTask() const
	{
		return dynamic_cast<const T*>(m_task.get());
	}

private:
    IMameClientSite &               m_site;
    std::unique_ptr<Task>           m_task;
    std::shared_ptr<wxProcess>      m_process;
    std::thread                     m_thread;

    static Job                      s_job;

	void Abort();
};

#endif // CLIENT_H
