/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include <QEvent>

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
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> ListXmlTask
class ListXmlTask : public Task
{
public:
	class Test;

	// ctor
	ListXmlTask(QString &&output_filename);

protected:
	virtual QStringList getArguments(const Preferences &) const;
	virtual void process(QProcess &process, QObject &handler) override;
	virtual void abort() override;

private:
	// ======================> list_xml_exception
	class list_xml_exception : public std::exception
	{
	public:
		list_xml_exception(ListXmlResultEvent::Status status, QString &&message = QString());

		ListXmlResultEvent::Status	m_status;
		QString						m_message;
	};

	QString			m_output_filename;
	volatile bool	m_aborted;

	void internalProcess(QIODevice &process);
};

#endif // LISTXMLTASK_H
