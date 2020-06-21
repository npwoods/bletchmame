/***************************************************************************

    client.cpp

    MAME Executable Client

***************************************************************************/

#include "client.h"

#include <QThread>

#include <iostream>
#include <thread>

#include "client.h"
#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  CORE IMPLEMENTATION
//**************************************************************************

Job MameClient::s_job;


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameClient::MameClient(QObject &event_handler, const Preferences &prefs)
    : m_event_handler(event_handler)
	, m_prefs(prefs)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MameClient::~MameClient()
{
	abort();
    reset();
}


//-------------------------------------------------
//  launch (main thread)
//-------------------------------------------------

void MameClient::launch(Task::ptr &&task)
{
	// Sanity check; don't do anything if we already have a task
	if (m_task)
	{
		throw false;
	}

	// set things up
	m_task = std::move(task);
	m_thread = std::thread([this]() { taskThreadProc(); });
}


//-------------------------------------------------
//  taskThreadProc - worker thread
//-------------------------------------------------

void MameClient::taskThreadProc()
{
	// identify the program
	const QString &program = m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// get the arguments
	QStringList arguments = m_task->GetArguments(m_prefs);

	// slap on any extra arguments
	const QString &extra_arguments = m_prefs.GetMameExtraArguments();
	appendExtraArguments(arguments, extra_arguments);

	// set up the QProcess
	QProcess process;

	// set the callback
	auto finished_callback = [this](int exitCode, QProcess::ExitStatus exitStatus)
	{
		m_task->OnChildProcessCompleted(static_cast<Task::emu_error>(exitCode));
	};
	connect(&process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), finished_callback);

	// launch the process
	process.start(program, arguments);
	if (!process.pid() || !process.waitForStarted() || !process.waitForReadyRead())
	{
		// TODO - better error handling, especially when we're not pointed at the proper executable
		throw false;
	}

	// add the process to the job
	s_job.AddProcess(process.pid());

	// invoke the task's process method
	m_task->Process(process, m_event_handler);

	// unlike wxWidgets, Qt whines with warnings if you destroy a QProcess before waiting
	// for it to exit, so we need to go through these steps here
	const int delayMilliseconds = 1000;
	if (!process.waitForFinished(delayMilliseconds))
	{
		process.kill();
		process.waitForFinished(delayMilliseconds);
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
//  reset
//-------------------------------------------------

void MameClient::reset()
{
    if (m_thread.joinable())
        m_thread.join();
    if (m_task)
        m_task.reset();
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void MameClient::abort()
{
	if (m_task)
		m_task->Abort();
}
