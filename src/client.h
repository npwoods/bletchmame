/***************************************************************************

    client.h

    Client for invoking MAME for various tasks

***************************************************************************/

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <thread>
#include <QProcess>

#include "task.h"
#include "job.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class MameClient : QObject
{
public:
	class Test;

	MameClient(QObject &event_handler, const Preferences &prefs);
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
	QObject &						m_event_handler;
	const Preferences &				m_prefs;
	Task::ptr						m_task;
	std::shared_ptr<QProcess>		m_process;
	std::thread						m_thread;

	static Job						s_job;

	static void appendExtraArguments(QStringList &argv, const QString &extraArguments);
};

#endif // CLIENT_H
