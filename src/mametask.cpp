/***************************************************************************

	mametask.cpp

	Abstract base class for asynchronous tasks that invoke MAME

***************************************************************************/

#include <QCoreApplication>

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
	: m_readySemaphoreReleased(false)
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

	// bail now if this can't meaningfully work
	if (program.isEmpty())
		return;

	// get the arguments
	QStringList arguments = getArguments(prefs);

	// slap on any extra arguments
	const QString &extraArguments = prefs.getMameExtraArguments();
	appendExtraArguments(arguments, extraArguments);

	// set up signals
	connect(&m_process, &QProcess::finished, &m_process, [this](int exitCode, QProcess::ExitStatus exitStatus)
	{
		onChildProcessCompleted(static_cast<MameTask::EmuError>(exitCode));
	});
	connect(&m_process, &QProcess::readyReadStandardOutput, &m_process, [this]()
	{
		releaseReadySemaphore();
	});
	m_process.setReadChannel(QProcess::StandardOutput);

	// log the command line (if appropriate)
	if (LOG_LAUNCH_COMMAND)
	{
		QFile file(QCoreApplication::applicationDirPath() + "/mame_command_line.txt");
		if (file.open(QIODevice::WriteOnly))
		{
			QTextStream textStream(&file);
			formatCommandLine(textStream, program, arguments);
		}
	}

	// launch the process
	m_process.start(program, arguments);

	// add the process to the job
	qint64 processId = m_process.processId();
	if (processId)
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

void MameTask::process(QObject &eventHandler)
{
	// wait for us to be ready
	m_readySemaphore.acquire(1);

	// use our own process call
	process(m_process, eventHandler);
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void MameTask::abort()
{
	Task::abort();
	releaseReadySemaphore();
}


//-------------------------------------------------
//  onChildProcessCompleted
//-------------------------------------------------

void MameTask::onChildProcessCompleted(EmuError status)
{
	releaseReadySemaphore();
}


//-------------------------------------------------
//  releaseReadySemaphore
//-------------------------------------------------

void MameTask::releaseReadySemaphore()
{
	// there are a number of activities that can trigger the need to release this semaphore (MAME completing, being aborted, or
	// error conditions); we want to make sure that all of these activities result the semaphore being released, and only once
	if (!m_readySemaphoreReleased)
	{
		m_readySemaphore.release(1);
		m_readySemaphoreReleased = true;
	}
}
