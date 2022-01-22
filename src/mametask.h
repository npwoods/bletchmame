/***************************************************************************

    mametask.h

    Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

#pragma once

#ifndef MAMETASK_H
#define MAMETASK_H

#include <QMutex>

#include "task.h"
#include "job.h"


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

	enum class EmuError
	{
		// standard MAME error codes, from MAME main.h
		None = 0,				// no error
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

	// start this task (should only be invoked from TaskClient from the main thread)
	virtual void start(Preferences &prefs) override final;

	// perform actual processing (should only be invoked from TaskClient within the thread proc)
	virtual void process(const PostEventFunc &postEventFunc) override final;

protected:
	// ctor
	MameTask();

	// if there is an active 
	void killActiveEmuProcess();

	// retrieves the arguments to be used at the command line
	virtual QStringList getArguments(const Preferences &prefs) const = 0;

	// called on a child thread tasked with ownership of a MAME child process
	virtual void process(QProcess &process, const PostEventFunc &postEventFunc) = 0;

	// called on the main thread when the child process has been completed
	virtual void onChildProcessCompleted(EmuError status);

private:
	class ProcessLocker;

	static Job		s_job;
	QString			m_program;
	QStringList		m_arguments;
	QProcess *		m_activeProcess;
	QMutex			m_activeProcessMutex;

	static void appendExtraArguments(QStringList &argv, const QString &extraArguments);
	static void formatCommandLine(QTextStream &stream, const QString &program, const QStringList &arguments);
};

#endif // MAMETASK_H
