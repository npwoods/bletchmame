/***************************************************************************

	mametask.cpp

	Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

#include <QCoreApplication>
#include <QMutexLocker>

#include "mametask.h"
#include "prefs.h"


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
//  start
//-------------------------------------------------

void MameTask::start(Preferences &prefs)
{
	Task::start(prefs);

	// identify the program
	m_program = prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// bail now if this can't meaningfully work
	if (m_program.isEmpty())
		return;

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
//  process
//-------------------------------------------------

void MameTask::process(const PostEventFunc &postEventFunc)
{
	// start the process
	QProcess emuProcess;
	emuProcess.setReadChannel(QProcess::StandardOutput);
	emuProcess.start(m_program, m_arguments);

	// add the process to the job
	qint64 processId = emuProcess.processId();
	if (processId)
		s_job.addProcess(processId);

	// set up the process locker
	ProcessLocker processLocker(*this, emuProcess);

	// wait for readyReadStandardOutput
	bool waitingComplete = false;
	connect(&emuProcess, &QProcess::finished, &emuProcess, [this, &waitingComplete](int exitCode, QProcess::ExitStatus exitStatus)
	{
		waitingComplete = true;
		onChildProcessCompleted((EmuError)exitCode);
	});
	connect(&emuProcess, &QProcess::readyReadStandardOutput, &emuProcess, [&waitingComplete]()
	{
		waitingComplete = true;
	});
	while (!waitingComplete)
		QCoreApplication::processEvents();

	// use our own process call
	process(emuProcess, postEventFunc);
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


//-------------------------------------------------
//  onChildProcessCompleted
//-------------------------------------------------

void MameTask::onChildProcessCompleted(EmuError status)
{
}
