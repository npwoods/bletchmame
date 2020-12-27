/***************************************************************************LOG_RESPONSES

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

#include <QTextStream>
#include <QWidget>
#include <QCoreApplication>
#include <QDir>
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

RunMachineTask::RunMachineTask(info::machine machine, QString &&software, std::map<QString, QString> &&slotOptions, QWidget &targetWindow)
    : m_machine(machine)
	, m_software(std::move(software))
	, m_slotOptions(std::move(slotOptions))
    , m_attachWindowParameter(getAttachWindowParameter(targetWindow))
	, m_chatterEnabled(false)
	, m_startedWithHashPaths(false)
{
}


//-------------------------------------------------
//  getArguments
//-------------------------------------------------

QStringList RunMachineTask::getArguments(const Preferences &prefs) const
{
	// somewhat of a hack - record whether we started with hash paths
	m_startedWithHashPaths = false;
	for (const QString &path : prefs.GetSplitPaths(Preferences::global_path_type::HASH))
	{
		if (QDir(path).exists())
		{
			m_startedWithHashPaths = true;
			break;
		}
	}

	// the first argument is the machine name
	QStringList results = { getMachine().name() };

	// the second argument is the software (if specified)
	if (!m_software.isEmpty())
		results.push_back(m_software);

	// then follow this with slot options
	for (const auto &opt : m_slotOptions)
	{
		results.push_back(QString("-") + opt.first);
		results.push_back(opt.second);
	}

	// and the rest of them
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
		"-cheatpath",
		prefs.GetGlobalPathWithSubstitutions(Preferences::global_path_type::CHEATS),
		"-window",
		"-keyboardprovider",
		"dinput",
		"-mouseprovider",
		"dinput",
		"-lightgunprovider",
		"dinput",
#if HAS_ATTACH_WINDOW
		"-attach_window",
		m_attachWindowParameter,
#endif // HAS_ATTACH_WINDOW
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

	// set up the controller
	MameWorkerController controller(process, [this, &handler](MameWorkerController::ChatterType type, const QString &text)
	{
		if (m_chatterEnabled)
		{
			auto evt = std::make_unique<ChatterEvent>(type, text);
			QCoreApplication::postEvent(&handler, evt.release());
		}
	});

	// receive the inaugural response from MAME; we want to call it quits if this doesn't work
	MameWorkerController::Response response = receiveResponseAndHandleUpdates(controller, handler);
	if (response.m_type != MameWorkerController::Response::Type::Ok)
	{
		// alas, we have an error starting MAME
		success = false;

		// try various strategies to get the best error message possible (perferring
		// one returned by the plugin)
		errorMessage = !response.m_text.isEmpty()
			? std::move(response.m_text)
			: controller.scrapeMameStartupError();
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
				controller.issueCommand(message.m_command);

				// and receive a response from MAME
				response = receiveResponseAndHandleUpdates(controller, handler);
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
//  receiveResponseAndHandleUpdates
//-------------------------------------------------

MameWorkerController::Response RunMachineTask::receiveResponseAndHandleUpdates(MameWorkerController &controller, QObject &handler)
{
	// get the response
	MameWorkerController::Response response = controller.receiveResponse();

	// did we get a status update
	if (response.m_update)
	{
		auto evt = std::make_unique<StatusUpdateEvent>(std::move(*response.m_update));
		QCoreApplication::postEvent(&handler, evt.release());
	}

	// return it either way
	return response;
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

ChatterEvent::ChatterEvent(MameWorkerController::ChatterType type, const QString &text)
	: QEvent(s_eventId) 
	, m_type(type)
	, m_text(text)
{
	// remove line endings from the text
	while (!m_text.isEmpty() && (m_text[m_text.size() - 1] == '\r' || m_text[m_text.size() - 1] == '\n'))
		m_text.resize(m_text.size() - 1);
}
