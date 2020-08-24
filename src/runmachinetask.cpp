/***************************************************************************

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

#include <QTextStream>
#include <QWidget>
#include <QCoreApplication>
#include <thread>

#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "prefs.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_RECEIVE			0
#define LOG_POST			0
#define LOG_COMMANDS		0
#define LOG_RESPONSES		0


//**************************************************************************
//  VARIABLES
//**************************************************************************

QEvent::Type RunMachineCompletedEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type StatusUpdateEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type ChatterEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, QString &&software, QWidget &targetWindow)
    : m_machine(machine)
	, m_software(software)
    , m_attachWindowParameter(getAttachWindowParameter(targetWindow))
	, m_chatterEnabled(false)
{
}


//-------------------------------------------------
//  getArguments
//-------------------------------------------------

QStringList RunMachineTask::getArguments(const Preferences &prefs) const
{
	QStringList results = { getMachine().name() };
	if (!m_software.isEmpty())
		results.push_back(m_software);

	QStringList args =
	{
		"-rompath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::ROMS),
		"-samplepath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::SAMPLES),
		"-cfg_directory",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::CONFIG),
		"-nvram_directory",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::NVRAM),
		"-hashpath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::HASH),
		"-artpath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::ARTWORK),
		"-pluginspath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::PLUGINS),
		"-window",
		"-keyboardprovider",
		"dinput",
		"-mouseprovider",
		"dinput",
		"-lightgunprovider",
		"dinput",
#ifdef Q_OS_WIN32
		"-attach_window",
		m_attachWindowParameter,
#endif
		"-skip_gameinfo",
		"-nomouse",
		"-debug",
		"-plugin",
		WORKER_UI_PLUGIN_NAME
	};

	results.append(args);
	return results;
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void RunMachineTask::abort()
{
	issue({ "exit" });
}


//-------------------------------------------------
//  onChildProcessCompleted
//-------------------------------------------------

void RunMachineTask::onChildProcessCompleted(emu_error status)
{
	internalPost(Message::type::TERMINATED, "", status);
}


//-------------------------------------------------
//  onChildProcessKilled
//-------------------------------------------------

void RunMachineTask::onChildProcessKilled()
{
	internalPost(Message::type::TERMINATED, "", emu_error::KILLED);
}


//-------------------------------------------------
//  issue
//-------------------------------------------------

void RunMachineTask::issue(const std::vector<QString> &args)
{
	QString command = buildCommand(args);
	internalPost(Message::type::COMMAND, std::move(command));
}


//-------------------------------------------------
//  issueFullCommandLine
//-------------------------------------------------

void RunMachineTask::issueFullCommandLine(QString &&fullCommand)
{
	fullCommand += "\r\n";
	internalPost(Message::type::COMMAND, std::move(fullCommand));
}


//-------------------------------------------------
//  internalPost
//-------------------------------------------------

void RunMachineTask::internalPost(Message::type type, QString &&command, emu_error status)
{
	if (LOG_POST)
		qDebug("RunMachineTask::internalPost(): command='%s'", command.trimmed().toStdString().c_str());

	Message message;
	message.m_type = type;
	message.m_command = std::move(command);
	message.m_status = status;
	m_messageQueue.post(std::move(message));
}


//-------------------------------------------------
//  buildCommand
//-------------------------------------------------

QString RunMachineTask::buildCommand(const std::vector<QString> &args)
{
	QString command;
	for (const QString &arg : args)
	{
		if (!command.isEmpty())
			command += " ";

		// do we need quotes?
		bool needs_quotes = arg.isEmpty() || arg.indexOf(' ') >= 0;

		// append the argument, with quotes if necessary
		if (needs_quotes)
			command += "\"";
		command += arg;
		if (needs_quotes)
			command += "\"";
	}
	command += "\r\n";
	return command;
}


//-------------------------------------------------
//  getAttachWindowParameter - determine the
//	parameter to pass to '-attach_window'
//-------------------------------------------------

QString RunMachineTask::getAttachWindowParameter(const QWidget &targetWindow)
{
	// the documentation for QWidget::WId() says that this value can change any
	// time; this is probably not true on Windows (where this returns the HWND)
	return QString::number(targetWindow.winId());
}


//**************************************************************************
//  CLIENT THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  process
//-------------------------------------------------

void RunMachineTask::process(QProcess &process, QObject &handler)
{
	bool success;
	QString errorMessage;
	Response response;

	// receive the inaugural response from MAME; we want to call it quits if this doesn't work
	response = receiveResponse(handler, process);
	if (response.m_type != Response::type::OK)
	{
		// alas, we have an error starting MAME
		success = false;

		// try various strategies to get the best error message possible
		if (!response.m_text.isEmpty())
		{
			// we got an error message from the LUA worker_ui plug-in; display it
			errorMessage = std::move(response.m_text);
		}
		else
		{
			// we did not get an error message from the LUA worker_ui plug-in; capture
			// MAME's standard output and present it (not ideal, but better than nothing)
			QByteArray errorOutput = process.readAllStandardError();
			errorMessage = errorOutput.length() > 0
				? QString("Error starting MAME:\r\n\r\n%1").arg(QString::fromUtf8(errorOutput))
				: QString("Error starting MAME");
		}
	}
	else
	{
		// loop until the process terminates
		bool done = false;
		emu_error status = emu_error::NONE;
		while (!done)
		{
			// await a message from the queue
			if (LOG_RECEIVE)
				qDebug("RunMachineTask::process(): invoking MessageQueue::receive()");
			Message message = m_messageQueue.receive();

			switch (message.m_type)
			{
			case Message::type::COMMAND:
				// emit this command to MAME
				if (LOG_COMMANDS)
					qDebug("RunMachineTask::process(): command='%s'", message.m_command.trimmed().toStdString().c_str());
				process.write(message.m_command.toUtf8());

				if (m_chatterEnabled)
					postChatter(handler, ChatterEvent::ChatterType::COMMAND_LINE, std::move(message.m_command));

				// and receive a response from MAME
				response = receiveResponse(handler, process);
				break;

			case Message::type::TERMINATED:
				done = true;
				status = message.m_status;
				break;

			default:
				throw false;
			}
		}

		// was there an error?
		if (status != emu_error::NONE)
		{
			// if so, capture what was emitted by MAME's standard output stream
			QByteArray errorMessageBytes = process.readAllStandardError();
			errorMessage = errorMessageBytes.length() > 0
				? QString::fromUtf8(errorMessageBytes)
				: QString("Error %1 running MAME").arg(QString::number((int)status));
		}
	}
	auto evt = std::make_unique<RunMachineCompletedEvent>(success, std::move(errorMessage));
	QCoreApplication::postEvent(&handler, evt.release());
}


//-------------------------------------------------
//  postChatter
//-------------------------------------------------

void RunMachineTask::postChatter(QObject &handler, ChatterEvent::ChatterType type, QString &&text)
{
	auto evt = std::make_unique<ChatterEvent>(type, std::move(text));
	QCoreApplication::postEvent(&handler, evt.release());
}


//-------------------------------------------------
//  receiveResponse
//-------------------------------------------------

RunMachineTask::Response RunMachineTask::receiveResponse(QObject &handler, QProcess &process)
{
	static const util::enum_parser<Response::type> s_response_type_parser =
	{
		{ "@OK", Response::type::OK, },
		{ "@ERROR", Response::type::ERR }
	};
	Response response;

	// This logic is complicated for two reasons:
	//
	//	1.  MAME has a pesky habit of emitting human readable messages to standard output, therefore
	//		we have a convention with the worker_ui plugin by which actual messages are preceeded with
	//		an at-sign
	//
	//	2.  Qt's stream classes are weird, hence the existance of reallyReadLineFromProcess()
	QString str;
	do
	{
		str = reallyReadLineFromProcess(process);
	} while (!str.isEmpty() && str[0] != '@');

	// special case; check for EOF
	if (str.isEmpty())
	{
		response.m_type = Response::type::END_OF_FILE;
		return response;
	}

	// log if debugging
	if (LOG_RESPONSES)
		qDebug("RunMachineTask::receiveResponse(): received '%s'", str.trimmed().toStdString().c_str());

	// chatter
	if (m_chatterEnabled)
		postChatter(handler, ChatterEvent::ChatterType::RESPONSE, QString(str));

	// start interpreting the response; first get the text
	int index = str.indexOf('#');
	if (index > 0)
	{
		// get the response text
		int response_text_position = index + 1;
		while (response_text_position < str.size() && str[response_text_position] == '#')
			response_text_position++;
		while (response_text_position < str.size() && str[response_text_position] == ' ')
			response_text_position++;
		response.m_text = str.right(str.length() - response_text_position);

		// and resize the original string
		str.resize(index);
	}

	// now get the arguments; there should be at least one
	std::vector<QString> args = util::string_split(str, [](auto ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });
	assert(!args.empty());

	// interpret the main message
	s_response_type_parser(args[0].toStdString(), response.m_type);

	// did we get a status reponse
	if (response.m_type == Response::type::OK && args.size() >= 2 && args[1] == "STATUS")
	{
		status::update statusUpdate = readStatus(process);
		auto evt = std::make_unique<StatusUpdateEvent>(std::move(statusUpdate));
		QCoreApplication::postEvent(&handler, evt.release());
	}
	return response;
}


//-------------------------------------------------
//  readStatus - read status XML from a process
//-------------------------------------------------

status::update RunMachineTask::readStatus(QProcess &process)
{
	bool done = false;
	QByteArray buffer;

	// because XmlParser::parse() is not smart enough to read until XML ends, we are using this
	// crude mechanism to read the XML while leaving everything else intact
	while (!done)
	{
		QString line = reallyReadLineFromProcess(process);
		buffer.append(line.toUtf8());

		{
			auto x = QString::fromUtf8(buffer).toStdString();
			auto y = x;
		}

		if (line.isEmpty() || line.startsWith("</"))
			done = true;
	}

	// now that we have our own private buffer, read it
	QDataStream stream(buffer);
	return status::update::read(stream);
}


//-------------------------------------------------
//  reallyReadLineFromProcess - read a line from
//	a QProcess, all the while attempting to accomodate
//	the behavior of QProcess
//-------------------------------------------------

QString RunMachineTask::reallyReadLineFromProcess(QProcess &process)
{
	// Qt's stream classes are very clunky.  There seems to be an assumption that the caller
	// wants something very simple (e.g. - read all input from a process), or wants something
	// very asynchronous event driven
	//
	// The consequence is that there are a bunch of unwanted behaviors from our perspective (as
	// a thread synchronously interacting with MAME) and this method is an attempt to isolate
	// these behaviors to provide an illusion of a simple, blocking text reader

	// loop while the process is running - because readLine() can return an empty string we
	// need to keep on tryin'
	QString result;
	while(result.isEmpty() && process.state() == QProcess::ProcessState::Running)
	{
		if (!process.canReadLine())
		{
			// yes Qt, we _really_ want to block!  whole heartedly!  but at the same time we
			// don't want to block forever without any escape
			process.waitForReadyRead(50);
		}

		// read a line if we can
		if (process.canReadLine())
			result = process.readLine();
	}
	return result;
}


//-------------------------------------------------
//  RunMachineCompletedEvent ctor
//-------------------------------------------------

RunMachineCompletedEvent::RunMachineCompletedEvent(bool success, QString &&errorMessage)
	: QEvent(s_eventId)
	, m_success(success)
	, m_errorMessage(errorMessage)
{
}


//-------------------------------------------------
//  StatusUpdateEvent ctor
//-------------------------------------------------

StatusUpdateEvent::StatusUpdateEvent(status::update &&update)
	: QEvent(s_eventId)
	, m_update(std::move(update))
{
}


//-------------------------------------------------
//  ChatterEvent ctor
//-------------------------------------------------

ChatterEvent::ChatterEvent(ChatterType type, QString &&text)
	: QEvent(s_eventId) 
	, m_type(type)
	, m_text(std::move(text))
{
	// remove line endings from the text
	while (!m_text.isEmpty() && (m_text[m_text.size() - 1] == '\r' || m_text[m_text.size() - 1] == '\n'))
		m_text.resize(m_text.size() - 1);
}
