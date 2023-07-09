/***************************************************************************

    mametask.h

    Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

#pragma once

#ifndef MAMETASK_H
#define MAMETASK_H

// bletchmame headers
#include "task.h"
#include "job.h"

// Qt headers
#include <QMutex>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

QT_BEGIN_NAMESPACE
class QProcess;
class QTextStream;
QT_END_NAMESPACE

// ======================> MameTask

class MameTask : public Task
{
public:
	class Test;

	enum class EmuExitCode
	{
		// standard MAME status/error codes, from MAME main.h
		Success = 0,			// no error
		FailedValidity = 1,		// failed validity checks
		MissingFiles = 2,		// missing files
		FatalError = 3,			// some other fatal error
		Device = 4,				// device initialization error (MESS-specific)
		NoSuchGame = 5,			// game was specified but doesn't exist
		InvalidConfig = 6,		// some sort of error in configuration 
		IdentNonRoms = 7,		// identified all non-ROM files
		IdentPartial = 8,		// identified some files but not all
		IdentNone = 9,			// identified no files

		// our error codes
		Invalid = 1000,			// invalid error code
		Killed,					// the process was killed
	};

protected:
	// ctor
	MameTask();

	// if there is an active 
	void killActiveEmuProcess();

	// prepares this task (should only be invoked from TaskDispatcher from the main thread)
	virtual void prepare(Preferences &prefs, EventHandlerFunc &&eventHandler) override final;

	// runs the task
	virtual void run() override final;

	// retrieves the arguments to be used at the command line
	virtual QStringList getArguments(const Preferences &prefs) const = 0;

	// called on a child thread tasked with ownership of a MAME child process
	virtual void run(std::optional<QProcess> &process) = 0;

	// accesses the exit code
	const std::optional<EmuExitCode> &emuExitCode() const { return m_emuExitCode; }

private:
	class ProcessLocker;

	static Job					s_job;
	QString						m_program;
	QStringList					m_arguments;
	QProcess *					m_activeProcess;
	QMutex						m_activeProcessMutex;
	std::optional<EmuExitCode>	m_emuExitCode;

	static void appendExtraArguments(QStringList &argv, const QString &extraArguments);
	static QString formatCommandLine(const QString &program, const QStringList &arguments);
};

#endif // MAMETASK_H
