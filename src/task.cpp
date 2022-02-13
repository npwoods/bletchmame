/***************************************************************************

    task.cpp

    Abstract base class for asynchronous tasks

***************************************************************************/

// bletchmame headers
#include "task.h"

// Qt headers
#include <QCoreApplication>
#include <QEvent>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  prepare
//-------------------------------------------------

void Task::prepare(Preferences &prefs, EventHandlerFunc &&eventHandler)
{
	m_eventHandler = std::move(eventHandler);
}


//-------------------------------------------------
//  postEventToHost
//-------------------------------------------------

void Task::postEventToHost(std::unique_ptr<QEvent> &&event)
{
	if (m_eventHandler)
		m_eventHandler(std::move(event));
}


//-------------------------------------------------
//  postEventToTask
//-------------------------------------------------

void Task::postEventToTask(std::unique_ptr<QEvent> &&event)
{
	QCoreApplication::postEvent(this, event.release());
}
