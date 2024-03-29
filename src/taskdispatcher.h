﻿/***************************************************************************

    taskdispatcher.h

    Client for executing async tasks (invoking MAME, media audits etc)

***************************************************************************/

#pragma once

#ifndef TASKDISPATCHER_H
#define TASKDISPATCHER_H

// bletchmame headers
#include "task.h"
#include "job.h"

// Qt headers
#include <QEvent>
#include <QSemaphore>
#include <QMutex>

// standard headers
#include <iostream>
#include <thread>

QT_BEGIN_NAMESPACE
class QProcess;
QT_END_NAMESPACE


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> FinalizeTaskEvent

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


// ======================> TaskDispatcher

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
		for (const Task::ptr &activeTask : m_activeTasks)
		{
			std::shared_ptr<T> ptr = dynamic_pointer_cast<T>(activeTask);
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
		for (const Task::ptr &activeTask : m_activeTasks)
		{
			std::shared_ptr<T> ptr = dynamic_pointer_cast<T>(activeTask);
			if (ptr)
				result++;
		}
		return result;
	}

private:
	QObject &				m_eventHandler;
	Preferences &			m_prefs;
	std::vector<Task::ptr>	m_activeTasks;

	void joinTask(Task &task);
};

#endif // TASKDISPATCHER_H
