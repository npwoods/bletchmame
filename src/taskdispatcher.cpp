/***************************************************************************

    taskdispatcher.cpp

    Client for executing async tasks (invoking MAME, media audits etc)

***************************************************************************/

#include <QCoreApplication>
#include <QDebug>
#include <QThread>

#include <iostream>
#include <thread>

#include "taskdispatcher.h"
#include "prefs.h"
#include "utility.h"


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
	for (const ActiveTask &activeTask : m_activeTasks)
		activeTask.m_task->abort();

	// now join all threads
	for (ActiveTask &activeTask : m_activeTasks)
	{
		if (activeTask.m_thread.joinable())
			activeTask.m_thread.join();
	}
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

	// set up an active task
	ActiveTask &activeTask = *m_activeTasks.emplace(m_activeTasks.end());
	activeTask.m_task = task;

	// start the task
	activeTask.m_task->start(m_prefs);

	// and perform processing on the thread proc
	activeTask.m_thread = std::thread([this, task{ std::move(task) }]
	{
		taskThreadProc(*task);
	});
}


//-------------------------------------------------
//  taskThreadProc
//-------------------------------------------------

void TaskDispatcher::taskThreadProc(Task &task)
{
	// do the heavy lifting
	task.process(m_eventHandler);

	// create another event to signal for this task to be finalized
	std::unique_ptr<QEvent> finalizeEvent = std::make_unique<FinalizeTaskEvent>(task);

	// and post it
	QCoreApplication::postEvent(&m_eventHandler, finalizeEvent.release());
}


//-------------------------------------------------
//  finalize - called to "garbage collect" a task
//	that has already completed and reported its
//	results
//-------------------------------------------------

void TaskDispatcher::finalize(const Task &task)
{
	// find the task
	auto iter = std::find_if(m_activeTasks.begin(), m_activeTasks.end(), [&task](const ActiveTask &activeTask)
	{
		return &*activeTask.m_task == &task;
	});

	// we expect this to be present
	if (iter == m_activeTasks.end())
		throw false;

	// join the thread if necessary
	if (iter->m_thread.joinable())
		iter->m_thread.join();

	// and remove it
	m_activeTasks.erase(iter);
}


//-------------------------------------------------
//  FinalizeTaskEvent ctor
//-------------------------------------------------

FinalizeTaskEvent::FinalizeTaskEvent(const Task &task)
	: QEvent(eventId())
	, m_task(task)
{
}
