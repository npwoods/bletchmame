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
//  VARIABLES
//**************************************************************************

QEvent::Type RunMachineResultEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type StatusUpdateEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type ChatterEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();


//**************************************************************************
//  UTILITY
//**************************************************************************

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


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, QString &&software, QWidget &targetWindow)
    : m_machine(machine)
	, m_software(software)
    , m_targetWindow(targetWindow)
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

	std::vector<QString> args =
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
		"-attach_window",
		QString::number((long) m_targetWindow.winId()),
		"-skip_gameinfo",
		"-nomouse",
		"-debug",
		"-plugin",
		WORKER_UI_PLUGIN_NAME
	};

	results.reserve(util::safe_static_cast<int>(results.size() + args.size()));
	for (QString &arg : args)
		results.push_back(std::move(arg));
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
	Message message;
	message.m_type = type;
	message.m_command = std::move(command);
	message.m_status = status;
	m_messageQueue.post(std::move(message));
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

	// set up streams (note that from our perspective, MAME's output streams are input
	// for us, and this we use QTextStream)
	QTextStream processStream(&process);

	// receive the inaugural response from MAME; we want to call it quits if this doesn't work
	response = receiveResponse(handler, processStream);
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
			Message message = m_messageQueue.receive();

			switch (message.m_type)
			{
			case Message::type::COMMAND:
				// emit this command to MAME
				process.write(message.m_command.toUtf8());

				if (m_chatterEnabled)
					postChatter(handler, ChatterEvent::ChatterType::COMMAND_LINE, std::move(message.m_command));

				// and receive a response from MAME
				response = receiveResponse(handler, processStream);
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
	auto evt = std::make_unique<RunMachineResultEvent>(success, std::move(errorMessage));
	QCoreApplication::postEvent(&handler, evt.release());
}


//-------------------------------------------------
//  receiveResponse
//-------------------------------------------------

RunMachineTask::Response RunMachineTask::receiveResponse(QObject &handler, QTextStream &processStream)
{
	static const util::enum_parser<Response::type> s_response_type_parser =
	{
		{ "@OK", Response::type::OK, },
		{ "@ERROR", Response::type::ERR }
	};
	Response response;

	// MAME has a pesky habit of emitting human readable messages to standard output, therefore
	// we have a convention with the worker_ui plugin by which actual messages are preceeded with
	// an at-sign
	QString str;
	do
	{
		str = processStream.readLine();
	} while ((!str.isEmpty() && str[0] != '@') || (str.isEmpty() && !processStream.atEnd()));

	// special case; check for EOF
	if (str.isEmpty())
	{
		response.m_type = Response::type::END_OF_FILE;
		return response;
	}

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
		status::update statusUpdate = status::update::read(processStream);
		auto evt = std::make_unique<StatusUpdateEvent>(std::move(statusUpdate));
		QCoreApplication::postEvent(&handler, evt.release());
	}
	return response;
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
//  RunMachineResultEvent ctor
//-------------------------------------------------

RunMachineResultEvent::RunMachineResultEvent(bool success, QString &&errorMessage)
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
	, m_text(text)
{
}
