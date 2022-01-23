/***************************************************************************

    task.h

    Abstract base class for asynchronous tasks

***************************************************************************/

#pragma once

#ifndef TASK_H
#define TASK_H

#include <memory>
#include <vector>

#include <QThread>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class Preferences;
class TaskDispatcher;

// ======================> Task

class Task : public QThread
{
	friend class TaskDispatcher;
public:
	typedef std::shared_ptr<Task> ptr;

	// ctor/dtor
	Task() = default;
	Task(const Task &) = delete;
	Task(Task &&) = delete;
	virtual ~Task() = default;

protected:
	typedef std::function<void(std::unique_ptr<QEvent> &&)> EventHandlerFunc;

	// prepares this task (should only be invoked from TaskDispatcher from the main thread)
	virtual void prepare(Preferences &prefs, EventHandlerFunc &&eventHandler);

	// posts an event to the host (the main thread) and the task (the client thread)
	void postEventToHost(std::unique_ptr<QEvent> &&event);
	void postEventToTask(std::unique_ptr<QEvent> &&event);

private:
	EventHandlerFunc	m_eventHandler;
};


#endif // TASK_H
