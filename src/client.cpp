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
//  TYPE DEFINITIONS
//**************************************************************************

#if 0
namespace
{
	class ClientProcess : public wxProcess
	{
	public:
		ClientProcess(std::function<void(Task::emu_error status)> func)
			: m_on_child_process_completed_func(std::move(func))
		{
		}

		void SetProcess(std::shared_ptr<wxProcess> &&process)
		{
			m_process = std::move(process);
		}

		virtual void OnTerminate(int pid, int status) override
		{
			wxLogStatus("Worker process terminated; pid=%d status=%d", pid, status);
			m_on_child_process_completed_func(static_cast<Task::emu_error>(status));
		}

	private:
		std::function<void(Task::emu_error status)>	m_on_child_process_completed_func;
		std::shared_ptr<wxProcess>					m_process;
	};
}
#endif


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
	Abort();
    Reset();
}



//-------------------------------------------------
//  Launch
//-------------------------------------------------

void MameClient::Launch(Task::ptr &&task)
{
	// Sanity check; don't do anything if we already have a task
	if (m_task)
	{
		throw false;
	}

	// identify the program
	const QString &program = m_prefs.GetGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// get the arguments
	QStringList arguments = task->GetArguments(m_prefs);

	// slap on any extra arguments
	const QString &extra_arguments = m_prefs.GetMameExtraArguments();
	appendExtraArguments(arguments, extra_arguments);

	// set up the QProcess
	auto process = std::make_shared<QProcess>();
	m_process = process;

	// set the callback
	auto finished_callback = [task](int exitCode, QProcess::ExitStatus exitStatus)
	{
		task->OnChildProcessCompleted(static_cast<Task::emu_error>(exitCode));
	};
	connect(process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), finished_callback);

	// launch the process
	process->start(program, arguments);
	if (!process->pid() || !process->waitForStarted() || !process->waitForReadyRead())
	{
		// TODO - better error handling, especially when we're not pointed at the proper executable
		throw false;
	}

	s_job.AddProcess(process->pid());

	m_task = std::move(task);
	m_thread = std::thread([process, this]()
	{
		// invoke the task's process method
		m_task->Process(*process, m_event_handler);
	});
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
//  Reset
//-------------------------------------------------

void MameClient::Reset()
{
    if (m_thread.joinable())
        m_thread.join();
    if (m_task)
        m_task.reset();

	if (m_process)
	{
		// unlike wxWidgets, Qt whines with warnings if you destroy a QProcess before waiting
		// for it to exit
		const int delayMilliseconds = 1000;
		if (!m_process->waitForFinished(delayMilliseconds))
		{
			m_process->kill();
			m_process->waitForFinished(delayMilliseconds);
		}
		m_process.reset();
	}
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void MameClient::Abort()
{
	if (m_task)
	{
		m_task->Abort();
	}
	if (m_process)
	{
		m_process->kill();
		m_task->OnChildProcessKilled();
	}
}
