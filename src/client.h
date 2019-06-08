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

class MameClient
{
public:
	MameClient(wxEvtHandler &event_handler, const Preferences &prefs);
	~MameClient();

	void Launch(Task::ptr &&task);
	void Reset();

	template<class T> T *GetCurrentTask()
	{
		return dynamic_cast<T*>(m_task.get());
	}
	template<class T> const T *GetCurrentTask() const
	{
		return dynamic_cast<const T*>(m_task.get());
	}

	bool IsTaskActive() const { return m_task.get() != nullptr; }

private:
	wxEvtHandler &					m_event_handler;
	const Preferences &				m_prefs;
	Task::ptr						m_task;
	std::shared_ptr<wxProcess>		m_process;
	std::thread						m_thread;

	static Job						s_job;

	void Abort();
};

#endif // CLIENT_H
