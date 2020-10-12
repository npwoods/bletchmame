/***************************************************************************

    runmachinetask.h

    Task for running an emulation session

***************************************************************************/

#pragma once

#ifndef RUNMACHINETASK_H
#define RUNMACHINETASK_H

#include <QEvent>
#include <optional>

#include "task.h"
#include "messagequeue.h"
#include "info.h"
#include "mameworkercontroller.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// workaround for GCC bug fixed in 7.4
#ifdef __GNUC__
#if __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#define SHOULD_BE_DELETE	default
#endif	// __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#endif // __GNUC__

#ifndef SHOULD_BE_DELETE
#define SHOULD_BE_DELETE	delete
#endif // !SHOULD_BE_DELETE

// the name of the worker_ui plugin
#define WORKER_UI_PLUGIN_NAME	"worker_ui"

// does this platform use -attach_window?
#ifdef Q_OS_WIN32
#define HAS_ATTACH_WINDOW	1
#else // !Q_OS_WIN32
#define HAS_ATTACH_WINDOW	0
#endif // Q_OS_WIN32


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> RunMachineCompletedEvent

class RunMachineCompletedEvent : public QEvent
{
public:
	RunMachineCompletedEvent(bool success, QString &&errorMessage);

	static QEvent::Type eventId()		{ return s_eventId; }
	bool success() const				{ return m_success; }
	const QString &errorMessage() const { return m_errorMessage; }

private:
	static QEvent::Type		s_eventId;
	bool					m_success;
    QString					m_errorMessage;
};


// ======================> StatusUpdateEvent

class StatusUpdateEvent : public QEvent
{
public:
	StatusUpdateEvent(status::update &&update);

	static QEvent::Type eventId() { return s_eventId; }
	status::update detachStatus() { return std::move(m_update); }

private:
	static QEvent::Type		s_eventId;
	status::update			m_update;
};


// ======================> ChatterEvent

class ChatterEvent : public QEvent
{
public:
	ChatterEvent(MameWorkerController::ChatterType type, const QString &text);

	static QEvent::Type eventId()					{ return s_eventId; }
	MameWorkerController::ChatterType type() const	{ return m_type; }
	const QString &text() const						{ return m_text; }

private:
	static QEvent::Type					s_eventId;
	MameWorkerController::ChatterType	m_type;
	QString								m_text;
};


// ======================> RunMachineTask

class RunMachineTask : public Task
{
public:
	class Test;

	RunMachineTask(info::machine machine, QString &&software, QWidget &targetWindow);

	void issue(const std::vector<QString> &args);
	void issueFullCommandLine(QString &&full_command);
	const info::machine &getMachine() const { return m_machine; }
	void setChatterEnabled(bool enabled) { m_chatterEnabled = enabled; }
	bool startedWithHashPaths() const { return m_startedWithHashPaths; }

protected:
	virtual QStringList getArguments(const Preferences &prefs) const override;
	virtual void process(QProcess &process, QObject &handler) override;
	virtual void abort() override;
	virtual void onChildProcessCompleted(emu_error status) override;
	virtual void onChildProcessKilled() override;

private:
    struct Message
    {
		enum class type
		{
			INVALID,
			COMMAND,
			TERMINATED
		};

		type						m_type;
		QString						m_command;
		emu_error					m_status;
    };

	info::machine					m_machine;
	QString							m_software;
	QString							m_attachWindowParameter;
	MessageQueue<Message>		    m_messageQueue;
	volatile bool					m_chatterEnabled;
	mutable bool					m_startedWithHashPaths;

	static QString buildCommand(const std::vector<QString> &args);
	static QString getAttachWindowParameter(const QWidget &targetWindow);

	void internalPost(Message::type type, QString &&command, emu_error status = emu_error::INVALID);
	static MameWorkerController::Response receiveResponseAndHandleUpdates(MameWorkerController &controller, QObject &handler);
};


#endif // RUNMACHINETASK_H
