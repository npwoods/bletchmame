/***************************************************************************LOG_RESPONSES

    runmachinetask.cpp

    Task for running an emulation session

***************************************************************************/

// bletchmame headers
#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"
#include "prefs.h"

// Qt headers
#include <QTextStream>
#include <QWidget>
#include <QCoreApplication>
#include <QDir>

// standard headers
#include <thread>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_RECEIVE			0
#define LOG_POST			0


// MAME requires different arguments on different platforms
#if defined(Q_OS_WIN32)

// Win32 declarations
#define KEYBOARD_PROVIDER	"dinput"
#define MOUSE_PROVIDER		"dinput"
#define LIGHTGUN_PROVIDER	"dinput"

#else

// Everything else
#define KEYBOARD_PROVIDER	""
#define MOUSE_PROVIDER		""
#define LIGHTGUN_PROVIDER	""

#endif


//**************************************************************************
//  TYPES
//**************************************************************************

namespace
{
	// ======================> IssueCommandEvent

	class IssueCommandEvent : public QEvent
	{
	public:
		IssueCommandEvent(QString &&command)
			: QEvent(s_eventId)
			, m_command(std::move(command))
		{
		}

		static QEvent::Type eventId() { return s_eventId; }
		QString &command() { return m_command; }

	private:
		static QEvent::Type		s_eventId;
		QString					m_command;
	};
}


//**************************************************************************
//  VARIABLES
//**************************************************************************

QEvent::Type RunMachineCompletedEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type StatusUpdateEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type ChatterEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();
QEvent::Type IssueCommandEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();


//**************************************************************************
//  MAIN THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

RunMachineTask::RunMachineTask(info::machine machine, QString &&software, std::map<QString, QString> &&slotOptions, QString &&attachWindowParameter)
    : m_machine(machine)
	, m_software(std::move(software))
	, m_slotOptions(std::move(slotOptions))
    , m_attachWindowParameter(std::move(attachWindowParameter))
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
	for (const QString &path : prefs.getSplitPaths(Preferences::global_path_type::HASH))
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

	// variable arguments
	if (strlen(KEYBOARD_PROVIDER) > 0)
		results << "-keyboardprovider" << KEYBOARD_PROVIDER;
	if (strlen(MOUSE_PROVIDER) > 0)
		results << "-mouseprovider" << MOUSE_PROVIDER;
	if (strlen(LIGHTGUN_PROVIDER) > 0)
		results << "-lightgunprovider" << LIGHTGUN_PROVIDER;
	if (!m_attachWindowParameter.isEmpty())
		results << "-attach_window" << m_attachWindowParameter;


	// and the rest of them
	results << "-rompath"			<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::ROMS);
	results << "-samplepath"		<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::SAMPLES);
	results << "-cfg_directory"		<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::CONFIG);
	results << "-nvram_directory"	<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::NVRAM);
	results << "-diff_directory"	<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::DIFF);
	results << "-hashpath"			<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::HASH);
	results << "-artpath"			<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::ARTWORK);
	results << "-pluginspath"		<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::PLUGINS);
	results << "-cheatpath"			<< prefs.getGlobalPathWithSubstitutions(Preferences::global_path_type::CHEATS);
	results << "-plugin"			<< WORKER_UI_PLUGIN_NAME;
	results << "-window";
	results << "-skip_gameinfo";
	results << "-nomouse";
	results << "-debug";

	return results;
}


//-------------------------------------------------
//  issue
//-------------------------------------------------

void RunMachineTask::issue(const std::vector<QString> &args)
{
	QString command = buildCommand(args);
	internalIssueCommand(std::move(command));
}


//-------------------------------------------------
//  issueFullCommandLine
//-------------------------------------------------

void RunMachineTask::issueFullCommandLine(QString &&fullCommand)
{
	fullCommand += "\r\n";
	internalIssueCommand(std::move(fullCommand));
}


//-------------------------------------------------
//  internalIssueCommand
//-------------------------------------------------

void RunMachineTask::internalIssueCommand(QString &&command)
{
	// empty commands are illegal
	assert(!command.isEmpty());

	if (LOG_POST)
		qDebug("RunMachineTask::internalIssueCommand(): command='%s'", command.trimmed().toStdString().c_str());

	// post this event to the task
	auto evt = std::make_unique<IssueCommandEvent>(std::move(command));
	postEventToTask(std::move(evt));
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


//**************************************************************************
//  CLIENT THREAD OPERATIONS
//**************************************************************************

//-------------------------------------------------
//  event
//-------------------------------------------------

bool RunMachineTask::event(QEvent *event)
{
	std::optional<bool> result = { };
	if (event->type() == IssueCommandEvent::eventId())
	{
		IssueCommandEvent &issueCommandEvent = *static_cast<IssueCommandEvent *>(event);
		m_commandQueue.push(std::move(issueCommandEvent.command()));
		result = true;
	}

	return result.has_value()
		? result.value()
		: MameTask::event(event);
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void RunMachineTask::run(QProcess &process)
{
	bool success;
	QString errorMessage;

	// set up the controller
	MameWorkerController controller(process, [this](MameWorkerController::ChatterType type, const QString &text)
	{
		if (m_chatterEnabled)
		{
			auto evt = std::make_unique<ChatterEvent>(type, text);
			postEventToHost(std::move(evt));
		}
	});

	// receive the inaugural response from MAME; we want to call it quits if this doesn't work
	MameWorkerController::Response response = receiveResponseAndHandleUpdates(controller);
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
		QString command;
		while (!(command = getNextCommand()).isEmpty())
		{
			// we've received a command
			if (LOG_RECEIVE)
				qDebug() << "RunMachineTask::run(): received command: " << command;

			// emit this command to MAME
			controller.issueCommand(command);

			// and receive a response from MAME
			response = receiveResponseAndHandleUpdates(controller);
		}

		// if we didn't get a MAME status code, sounds like we need to bump off MAME
		if (!emuExitCode().has_value())
		{
			killActiveEmuProcess();
			while (!emuExitCode().has_value())
				QCoreApplication::processEvents();
		}

		// was there an error?
		EmuExitCode exitCode = emuExitCode().value();
		if (exitCode != EmuExitCode::Success)
		{
			// if so, capture what was emitted by MAME's standard output stream
			QByteArray errorMessageBytes = process.readAllStandardError();
			errorMessage = errorMessageBytes.length() > 0
				? QString::fromUtf8(errorMessageBytes)
				: QString("Error %1 running MAME").arg(QString::number((int)exitCode));
		}
	}
	auto evt = std::make_unique<RunMachineCompletedEvent>(success, std::move(errorMessage));
	postEventToHost(std::move(evt));
}


//-------------------------------------------------
//  getNextCommand
//-------------------------------------------------

QString RunMachineTask::getNextCommand()
{
	// process events until we have something in the queue, or until we need to bail
	while(m_commandQueue.empty() && !isInterruptionRequested() && !emuExitCode().has_value())
		QCoreApplication::processEvents();

	// get the command if we have one
	QString result;
	if (!m_commandQueue.empty())
	{
		result = std::move(m_commandQueue.front());
		m_commandQueue.pop();
	}
	return result;
}


//-------------------------------------------------
//  receiveResponseAndHandleUpdates
//-------------------------------------------------

MameWorkerController::Response RunMachineTask::receiveResponseAndHandleUpdates(MameWorkerController &controller)
{
	// get the response
	MameWorkerController::Response response = controller.receiveResponse();

	// did we get a status update
	if (response.m_update)
	{
		auto evt = std::make_unique<StatusUpdateEvent>(std::move(*response.m_update));
		postEventToHost(std::move(evt));
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
