/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include "task.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// pesky macros
#ifdef ERROR
#undef ERROR
#endif // ERROR


//**************************************************************************
//  TYPES
//**************************************************************************

class ListXmlResultEvent : public QEvent
{
public:
	enum class Status
	{
		SUCCESS,		// the invocation of -listxml succeeded
		ABORTED,		// the user aborted the -listxml request midflight
		ERROR			// an error is to be reported to use user
	};

	// ctor
	ListXmlResultEvent(Status status, QString &&errorMessage);

	// accessors
	static QEvent::Type eventId()			{ return s_eventId; }
	Status status() const					{ return m_status; }
	const QString &	errorMessage() const	{ return m_errorMessage; }

private:
	static QEvent::Type	s_eventId;
	Status				m_status;
	QString				m_errorMessage;
};


//**************************************************************************
//  FUNCTION PROTOTYPES
//**************************************************************************

Task::ptr create_list_xml_task(QString &&dest);

#endif // LISTXMLTASK_H
