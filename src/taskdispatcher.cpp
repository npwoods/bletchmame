/***************************************************************************

    taskdispatcher.cpp

    Client for executing async tasks (invoking MAME, media audits etc)

***************************************************************************/

// bletchmame headers
#include "taskdispatcher.h"
#include "prefs.h"
#include "utility.h"

// Qt headers
#include <QCoreApplication>
#include <QDebug>
#include <QThread>

// standard headers
#include <iostream>
#include <thread>


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

QEvent::Type FinalizeTaskEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

TaskDispatcher::TaskDispatcher(QObject &eventHandler, Preferences &prefs)
	: m_eventHandler(eventHandler)
	, m_prefs(prefs)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

TaskDispatcher::~TaskDispatcher()
{
	// if we have any outstanding tasks, instruct all of them to abort
	for (const Task::ptr &task : m_activeTasks)
		task->requestInterruption();

	// now join all tasks
	for (const Task::ptr &task : m_activeTasks)
		joinTask(*task);
}


//-------------------------------------------------
//  launch
//-------------------------------------------------

void TaskDispatcher::launch(const Task::ptr &task)
{
	launch(Task::ptr(task));
}


//-------------------------------------------------
//  launch
//-------------------------------------------------

void TaskDispatcher::launch(Task::ptr &&task)
{
	assert(task);

	// add this to the active tasks
	m_activeTasks.push_back(std::move(task));

	// identify the active tasks
	Task &activeTask = *m_activeTasks.back();

	// prepare it
	auto postEventFunc = [this](std::unique_ptr<QEvent> &&event)
	{
		QCoreApplication::postEvent(&m_eventHandler, event.release());
	};
	activeTask.prepare(m_prefs, postEventFunc);

	// we want to fire an event when the task is finalized
	connect(&activeTask, &QThread::finished, &activeTask, [this, &activeTask]()
	{
		// create another event to signal for this task to be finalized
		std::unique_ptr<QEvent> finalizeEvent = std::make_unique<FinalizeTaskEvent>(activeTask);

		// and post it
		QCoreApplication::postEvent(&m_eventHandler, finalizeEvent.release());
	});

	// and start it up!
	activeTask.start();
}


//-------------------------------------------------
//  finalize - called to "garbage collect" a task
//	that has already completed and reported its
//	results
//-------------------------------------------------

void TaskDispatcher::finalize(const Task &task)
{
	// find the task
	auto iter = std::find_if(m_activeTasks.begin(), m_activeTasks.end(), [&task](const Task::ptr &thisTask)
	{
		return &*thisTask == &task;
	});

	// we expect this to be present
	if (iter != m_activeTasks.end())
	{
		// join the thread and remove it
		joinTask(**iter);
		m_activeTasks.erase(iter);
	}
}


//-------------------------------------------------
//  joinTask
//-------------------------------------------------

void TaskDispatcher::joinTask(Task &task)
{
	using namespace std::chrono_literals;

	if (!task.wait(QDeadlineTimer(5s)))
	{
		// something is very wrong if we got here
		task.terminate();
	}
}


//-------------------------------------------------
//  FinalizeTaskEvent ctor
//-------------------------------------------------

FinalizeTaskEvent::FinalizeTaskEvent(const Task &task)
	: QEvent(eventId())
	, m_task(task)
{
}
