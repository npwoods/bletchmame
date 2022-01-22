/***************************************************************************

    taskdispatcher.h

    Client for executing async tasks (invoking MAME, media audits etc)

***************************************************************************/

#pragma once

#ifndef TASKCLIENT_H
#define TASKCLIENT_H

#include <iostream>
#include <thread>

#include <QEvent>
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

// ======================> Task

class FinalizeTaskEvent : public QEvent
{
public:
	FinalizeTaskEvent(const Task &task);

	static QEvent::Type eventId() { return s_eventId; }
	const Task &task() const { return m_task; }

private:
	static QEvent::Type s_eventId;
	const Task &		m_task;
};


// ======================> Task

class TaskDispatcher : public QObject
{
public:
	class Test;

	TaskDispatcher(QObject &eventHandler, Preferences &prefs);
	~TaskDispatcher();

	// launches a task
	void launch(const Task::ptr &task);
	void launch(Task::ptr &&task);

	// finalizes a specific class
	void finalize(const Task &task);

	// gets all active tasks of a type
	template<class T>
	std::vector<std::shared_ptr<T>> getActiveTasksByType()
	{
		std::vector<std::shared_ptr<T>> results;
		for (const ActiveTask &activeTask : m_activeTasks)
		{
			std::shared_ptr<T> ptr = dynamic_pointer_cast<T>(activeTask.m_task);
			if (ptr)
				results.push_back(std::move(ptr));
		}
		return results;
	}

	// counts active tasks of a type
	template<class T>
	std::size_t countActiveTasksByType()
	{
		std::size_t result = 0;
		for (const ActiveTask &activeTask : m_activeTasks)
		{
			std::shared_ptr<T> ptr = dynamic_pointer_cast<T>(activeTask.m_task);
			if (ptr)
				result++;
		}
		return result;
	}

private:
	struct ActiveTask
	{
		Task::ptr					m_task;
		std::unique_ptr<QThread>	m_thread;

		void join();
	};

	QObject &					m_eventHandler;
	Preferences &				m_prefs;
	std::vector<ActiveTask>		m_activeTasks;

	// private methods
	void taskThreadProc(Task &task);
	static void appendExtraArguments(QStringList &argv, const QString &extraArguments);
};

#endif // TASKCLIENT_H
