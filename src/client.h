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

	// launches a task
	void Launch(Task::ptr &&task);

	// instructs the currently running task to stop, and ensures that
	// the task's premature termination will not cause problems
	void Abort();

	// called when a task's final event is received, and waits for the task
	// to complete
	void Reset();

	template<class T> std::shared_ptr<T> GetCurrentTask()
	{
		return std::dynamic_pointer_cast<T>(m_task);
	}
	template<class T> std::shared_ptr<const T> GetCurrentTask() const
	{
		return std::dynamic_pointer_cast<const T>(m_task);
	}

	bool IsTaskActive() const { return m_task.get() != nullptr; }

	template<class T> bool IsTaskActive() const { return dynamic_cast<T *>(m_task.get()) != nullptr; }

private:
	wxEvtHandler &					m_event_handler;
	const Preferences &				m_prefs;
	Task::ptr						m_task;
	std::shared_ptr<wxProcess>		m_process;
	std::thread						m_thread;

	static Job						s_job;
};

#endif // CLIENT_H
