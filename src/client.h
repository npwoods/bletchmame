/***************************************************************************

    client.h

    Client for invoking MAME for various tasks

***************************************************************************/

#pragma once

#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <thread>
#include <QSemaphore>
#include <QMutex>

#include "task.h"
#include "job.h"

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class MameClient : QObject
{
public:
	class Test;

	MameClient(QObject &eventHandler, const Preferences &prefs);
	~MameClient();

	// launches a task - only safe to run when there is no active task
	void launch(Task::ptr &&task);

	// instructs the currently running task to stop, and ensures that
	// the task's premature termination will not cause problems
	void abort();

	// called when a task's final event is received, and waits for the task
	// to complete
	void waitForCompletion();

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
	static Job						s_job;

	// variables configured at ctor
	QObject &						m_eventHandler;
	const Preferences &				m_prefs;
	QSemaphore						m_taskStartSemaphore;
	QMutex							m_processMutex;

	// runtime variables administered from the main thread
	Task::ptr						m_task;
	std::thread						m_workerThread;

	// runtime variables administered from the worker thread
	std::unique_ptr<QProcess>		m_process;

	// private methods
	void taskThreadProc();
	static void appendExtraArguments(QStringList &argv, const QString &extraArguments);
};

#endif // CLIENT_H
