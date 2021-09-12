/***************************************************************************

    listxmltask.h

    Task for invoking '-listxml'

***************************************************************************/

#pragma once

#ifndef LISTXMLTASK_H
#define LISTXMLTASK_H

#include <QEvent>

#include "mametask.h"


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
	ListXmlProgressEvent(QString &&machineName, QString &&machineDescription);

	// accessors
	static QEvent::Type eventId() { return s_eventId; }
	const QString &machineName() const { return m_machineName; }
	const QString &machineDescription() const { return m_machineDescription; }

private:
	static QEvent::Type	s_eventId;
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
	virtual void process(QProcess &process, QObject &handler) override final;
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

	QString			m_outputFilename;
	volatile bool	m_aborted;

	void internalProcess(QIODevice &process, const std::function<void(std::u8string_view, std::u8string_view)> &progressCallback = { });
};

#endif // LISTXMLTASK_H
