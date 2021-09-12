/***************************************************************************

    task.h

    Abstract base class for asynchronous tasks

***************************************************************************/

#pragma once

#ifndef TASK_H
#define TASK_H

#include <memory>
#include <vector>

#include <QObject>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class Preferences;
class TaskDispatcher;

// ======================> Task

class Task : public QObject
{
	friend class TaskDispatcher;
public:
	typedef std::shared_ptr<Task> ptr;

	// ctor/dtor
	Task();
	Task(const Task &) = delete;
	Task(Task &&) = delete;
	virtual ~Task();

	// accessors
	bool completed() const { return m_completed; }

	// called on the main thread to trigger a shutdown (e.g. - BletchMAME is closing)
	virtual void abort();

protected:
	// start this task (should only be invoked from TaskClient from the main thread)
	virtual void start(Preferences &prefs);

	// perform actual processing (should only be invoked from TaskClient within the thread proc)
	virtual void process(QObject &eventHandler) = 0;

private:
	volatile bool m_completed;
};


#endif // TASK_H
