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

	// called on the main thread to trigger a shutdown (e.g. - BletchMAME is closing)
	virtual void abort();

protected:
	typedef std::function<void(std::unique_ptr<QEvent> &&event)> PostEventFunc;

	// start this task (should only be invoked from TaskClient from the main thread)
	virtual void start(Preferences &prefs);

	// perform actual processing (should only be invoked from TaskClient within the thread proc)
	virtual void process(const PostEventFunc &postEventFunc) = 0;

	// are we aborting?
	bool hasAborted() const;

private:
	volatile bool m_hasAborted;
};


#endif // TASK_H
