/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

// bletchmame headers
#include "mametask.h"
#include "info_builder.h"

// Qt headers
#include <QEvent>


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

// ======================> ListXmlProgressEvent

class ListXmlProgressEvent : public QEvent
{
public:
	// ctor
	ListXmlProgressEvent(int machineCount, QString &&machineName, QString &&machineDescription);

	// accessors
	static QEvent::Type eventId() { return s_eventId; }
	int machineCount() const { return m_machineCount; }
	const QString &machineName() const { return m_machineName; }
	const QString &machineDescription() const { return m_machineDescription; }

private:
	static QEvent::Type	s_eventId;
	int					m_machineCount;
	QString				m_machineName;
	QString				m_machineDescription;
};


// ======================> ListXmlResultEvent

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

class ListXmlTask : public MameTask
{
public:
	class Test;

	// ctor
	ListXmlTask(QString &&outputFilename);

protected:
	virtual QStringList getArguments(const Preferences &) const override final;
	virtual void run(std::optional<QProcess> &process) override final;

private:
	// ======================> list_xml_exception
	class ListXmlError
	{
	public:
		ListXmlError(ListXmlResultEvent::Status status, QString &&message = QString());

		// accessors
		ListXmlResultEvent::Status status() const;
		QString &message();

	private:
		ListXmlResultEvent::Status	m_status;
		QString						m_message;
	};

	QString			m_outputFilename;

	std::optional<ListXmlError> internalRun(QIODevice &process, const info::database_builder::ProcessXmlCallback &progressCallback = { });
};

#endif // LISTXMLTASK_H
