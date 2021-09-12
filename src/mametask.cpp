/***************************************************************************

	mametask.cpp

	Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

#include "mametask.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_LAUNCH_COMMAND	0


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

Job MameTask::s_job;

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameTask::MameTask()
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

MameTask::~MameTask()
{
	// unlike wxWidgets, Qt whines with warnings if you destroy a QProcess before waiting
	// for it to exit, so we need to go through these steps here
	const int delayMilliseconds = 1000;
	if (!m_process.waitForFinished(delayMilliseconds))
	{
		m_process.kill();
		m_process.waitForFinished(delayMilliseconds);
	}
}


//-------------------------------------------------
//  start
//-------------------------------------------------

void MameTask::start(Preferences &prefs)
{
	Task::start(prefs);

	// identify the program
	const QString &program = prefs.getGlobalPath(Preferences::global_path_type::EMU_EXECUTABLE);

	// get the arguments
	QStringList arguments = getArguments(prefs);

	// slap on any extra arguments
	const QString &extraArguments = prefs.getMameExtraArguments();
	appendExtraArguments(arguments, extraArguments);

	// set the callback
	auto finishedCallback = [this](int exitCode, QProcess::ExitStatus exitStatus)
	{
		onChildProcessCompleted(static_cast<MameTask::EmuError>(exitCode));
	};
	connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), finishedCallback);
	m_process.setReadChannel(QProcess::StandardOutput);

	// log the command line (if appropriate)
	if (LOG_LAUNCH_COMMAND)
		qDebug() << "MameTask::prepare(): program=" << program << " arguments=" << arguments;

	// launch the process
	m_process.start(program, arguments);
	qint64 processId = m_process.processId();
	if (!processId || !m_process.waitForStarted() || !m_process.waitForReadyRead())
	{
		// TODO - better error handling, especially when we're not pointed at the proper executable
		qInfo() << "Failed to run MAME!\nstdout=" << m_process.readAllStandardOutput() << "\nstderr=" << m_process.readAllStandardError();
		throw false;
	}

	// add the process to the job
	s_job.addProcess(processId);
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
//  process
//-------------------------------------------------

void MameTask::process(QObject &eventHandler)
{
	// use our own process call
	process(m_process, eventHandler);
}


//-------------------------------------------------
//  onChildProcessCompleted
//-------------------------------------------------

void MameTask::onChildProcessCompleted(EmuError status)
{
}
