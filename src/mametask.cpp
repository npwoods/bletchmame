/***************************************************************************

	mametask.cpp

	Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

// bletchmame headers
#include "mametask.h"
#include "prefs.h"

// Qt headers
#include <QCoreApplication>
#include <QMutexLocker>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_LAUNCH_COMMAND	0


//**************************************************************************
//  TYPES
//**************************************************************************

class MameTask::ProcessLocker
{
public:
	ProcessLocker(MameTask &task, QProcess &process)
		: m_task(task)
	{
		QMutexLocker locker(&m_task.m_activeProcessMutex);
		m_task.m_activeProcess = &process;
	}

	~ProcessLocker()
	{
		QMutexLocker locker(&m_task.m_activeProcessMutex);
		m_task.m_activeProcess = nullptr;
	}

private:
	MameTask &	m_task;
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

Job MameTask::s_job;

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameTask::MameTask()
	: m_activeProcess(nullptr)
{
}


//-------------------------------------------------
//  prepare
//-------------------------------------------------

void MameTask::prepare(Preferences &prefs, EventHandlerFunc &&eventHandler)
{
	Task::prepare(prefs, std::move(eventHandler));

	// identify the program
	m_program = prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// get the arguments
	m_arguments = getArguments(prefs);

	// slap on any extra arguments
	const QString &extraArguments = prefs.getMameExtraArguments();
	appendExtraArguments(m_arguments, extraArguments);

	// log the command line (if appropriate)
	if (LOG_LAUNCH_COMMAND)
	{
		QFile file(QCoreApplication::applicationDirPath() + "/mame_command_line.txt");
		if (file.open(QIODevice::WriteOnly))
		{
			QTextStream textStream(&file);
			formatCommandLine(textStream, m_program, m_arguments);
		}
	}
}


//-------------------------------------------------
//  appendExtraArguments
//-------------------------------------------------

void MameTask::appendExtraArguments(QStringList &argv, const QString &extraArguments)
{
	std::optional<int> wordStartPos = { };
	bool inQuotes = false;
	for (int i = 0; i <= extraArguments.size(); i++)
	{
		if (i == extraArguments.size()
			|| (inQuotes && extraArguments[i] == '\"')
			|| (!inQuotes && extraArguments[i].isSpace()))
		{
			if (wordStartPos)
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
		else if (!inQuotes && !wordStartPos)
		{
			wordStartPos = i;
		}
	}
}


//-------------------------------------------------
//  formatCommandLine
//-------------------------------------------------

void MameTask::formatCommandLine(QTextStream &stream, const QString &program, const QStringList &arguments)
{
	bool isFirst = true;
	QStringList allParams;
	allParams << program << arguments;
	for (const QString &str : allParams)
	{
		if (isFirst)
			isFirst = false;
		else
			stream << ' ';

		bool needsQuotes = str.isEmpty() || str.indexOf(' ') >= 0;
		if (needsQuotes)
			stream << '\"';
		stream << str;
		if (needsQuotes)
			stream << '\"';
	}
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void MameTask::run()
{
	// start the process
	std::optional<QProcess> emuProcess;
	emuProcess.emplace();
	emuProcess->setReadChannel(QProcess::StandardOutput);
	emuProcess->start(m_program, m_arguments);

	// identify the process
	qint64 processId = emuProcess->processId();
	if (processId != 0)
	{
		// add it to the job
		s_job.addProcess(processId);

		// set up the process locker
		ProcessLocker processLocker(*this, emuProcess.value());

		// wait for readyReadStandardOutput
		bool waitingComplete = false;
		connect(&emuProcess.value(), &QProcess::finished, &emuProcess.value(), [this, &waitingComplete](int exitCode, QProcess::ExitStatus exitStatus)
		{
			waitingComplete = true;
			m_emuExitCode = exitStatus == QProcess::ExitStatus::NormalExit
				? (EmuExitCode)exitCode
				: EmuExitCode::Killed;
		});
		connect(&emuProcess.value(), &QProcess::readyReadStandardOutput, &emuProcess.value(), [&waitingComplete]()
		{
			waitingComplete = true;
		});
		while (!waitingComplete)
			QCoreApplication::processEvents();
	}
	else
	{
		// no PID? no process
		emuProcess.reset();
	}

	// use our own run call
	run(emuProcess);
}


//-------------------------------------------------
//  killActiveEmuProcess
//-------------------------------------------------

void MameTask::killActiveEmuProcess()
{
	QMutexLocker locker(&m_activeProcessMutex);
	if (m_activeProcess)
		m_activeProcess->kill();
}
