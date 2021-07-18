/***************************************************************************

    client.cpp

    MAME Executable Client

***************************************************************************/

#include <QDebug>
#include <QThread>

#include <iostream>
#include <thread>

#include "client.h"
#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_LAUNCH_COMMAND	0


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

Job MameClient::s_job;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameClient::MameClient(QObject &event_handler, const Preferences &prefs)
    : m_eventHandler(event_handler)
	, m_prefs(prefs)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MameClient::~MameClient()
{
	abort();
}


//-------------------------------------------------
//  launch (main thread)
//-------------------------------------------------

void MameClient::launch(Task::ptr &&task)
{
	// Sanity check; don't do anything if we already have a task
	if (m_task || m_workerThread.joinable() || m_process)
		throw false;

	// set things up
	m_task = std::move(task);
	m_workerThread = std::thread([this]() { taskThreadProc(); });

	// wait for the worker thread to start up and be ready
	m_taskStartSemaphore.acquire();
}


//-------------------------------------------------
//  taskThreadProc - worker thread
//-------------------------------------------------

void MameClient::taskThreadProc()
{
	// identify the program
	const QString &program = m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// get the arguments
	QStringList arguments = m_task->getArguments(m_prefs);

	// slap on any extra arguments
	const QString &extra_arguments = m_prefs.GetMameExtraArguments();
	appendExtraArguments(arguments, extra_arguments);

	// set up the QProcess
	{
		QMutexLocker locker(&m_processMutex);
		m_process = std::make_unique<QProcess>();
	}

	// set the callback
	auto finishedCallback = [this](int exitCode, QProcess::ExitStatus exitStatus)
	{
		m_task->onChildProcessCompleted(static_cast<Task::emu_error>(exitCode));
	};
	connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), finishedCallback);
	m_process->setReadChannel(QProcess::StandardOutput);

	// log the command line (if appropriate)
	if (LOG_LAUNCH_COMMAND)
		qDebug() << "MameClient::taskThreadProc(): program=" << program << " arguments=" << arguments;

	// launch the process
	m_process->start(program, arguments);
	qint64 processId = m_process->processId();
	if (!processId || !m_process->waitForStarted() || !m_process->waitForReadyRead())
	{
		// TODO - better error handling, especially when we're not pointed at the proper executable
		qInfo() << "Failed to run MAME!\nstdout=" << m_process->readAllStandardOutput() << "\nstderr=" << m_process->readAllStandardError();
		throw false;
	}

	// add the process to the job
	s_job.addProcess(processId);

	// we're done setting up; signal to the main thread
	m_taskStartSemaphore.release();

	// invoke the task's process method
	m_task->process(*m_process, m_eventHandler);

	// unlike wxWidgets, Qt whines with warnings if you destroy a QProcess before waiting
	// for it to exit, so we need to go through these steps here
	const int delayMilliseconds = 1000;
	if (!m_process->waitForFinished(delayMilliseconds))
	{
		m_process->kill();
		m_process->waitForFinished(delayMilliseconds);
	}

	// delete the process object
	{
		QMutexLocker locker(&m_processMutex);
		m_process.reset();
	}
}


//-------------------------------------------------
//  appendExtraArguments
//-------------------------------------------------

void MameClient::appendExtraArguments(QStringList &argv, const QString &extraArguments)
{
	std::optional<int> wordStartPos = { };
	bool inQuotes = false;
	for (int i = 0; i <= extraArguments.size(); i++)
	{
		if (i == extraArguments.size()
			|| (inQuotes && extraArguments[i] == '\"')
			|| (!inQuotes && extraArguments[i].isSpace()))
		{
			if (wordStartPos.has_value())
			{
				QString word = extraArguments.mid(wordStartPos.value(), i - wordStartPos.value());
				argv.append(std::move(word));
				wordStartPos = { };
			}
			inQuotes = false;
  		}
		else if (!inQuotes && extraArguments[i] == '\"')
		{
			inQuotes = true;
			wordStartPos = i + 1;
		}
		else if (!inQuotes && !wordStartPos.has_value())
		{
			wordStartPos = i;
		}
	}
}


//-------------------------------------------------
//  waitForCompletion
//-------------------------------------------------

void MameClient::waitForCompletion()
{
    if (m_workerThread.joinable())
		m_workerThread.join();
    if (m_task)
        m_task.reset();
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void MameClient::abort()
{
	// tell the task itself to abort
	if (m_task)
		m_task->abort();

	// kill the process
	{
		QMutexLocker locker(&m_processMutex);
		if (m_process)
		{
			m_process->kill();
			m_task->onChildProcessKilled();
		}
	}

	// finally just wait for completion
	waitForCompletion();
}
