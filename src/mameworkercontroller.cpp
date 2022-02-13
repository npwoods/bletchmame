/***************************************************************************

    mameworkercontroller.cpp

    Logic for issuing worker_ui commands and receiving responses

***************************************************************************/

// bletchmame headers
#include "mameworkercontroller.h"

// Qt headers
#include <QBuffer>
#include <QDateTime>
#include <QProcess>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define LOG_COMMANDS		0
#define LOG_RESPONSES		0
#define TIMEOUT_SECONDS		300


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MameWorkerController::MameWorkerController(QProcess &process, std::function<void(ChatterType, const QString &)> &&chatterCallback)
    : m_process(process)
	, m_chatterCallback(std::move(chatterCallback))
	, m_timedOut(false)
{
}


//-------------------------------------------------
//  receiveResponse
//-------------------------------------------------

MameWorkerController::Response MameWorkerController::receiveResponse()
{
	static const util::enum_parser<Response::Type> s_response_type_parser =
	{
		{ "@OK", Response::Type::Ok, },
		{ "@ERROR", Response::Type::Error }
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
		str = reallyReadLineFromProcess();
	} while (!str.isEmpty() && str[0] != '@');

	// special case; check for EOF
	if (str.isEmpty())
	{
		response.m_type = Response::Type::EndOfFile;
		return response;
	}

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
	}

	// now get the arguments; there should be at least one
	std::vector<QString> args = util::string_split(
		str.left(index >= 0 ? index : str.length()),
		[](auto ch) { return ch == ' ' || ch == '\r' || ch == '\n'; });
	assert(!args.empty());

	// interpret the main message
	std::string responseType = args[0].toStdString();
	s_response_type_parser(std::u8string_view((const char8_t *) responseType.data(), responseType.size()), response.m_type);

	// logging and chatter
	if (LOG_RESPONSES)
		qDebug("RunMachineTask::receiveResponse(): received '%s'", str.trimmed().toStdString().c_str());
	callChatterCallback(
		response.m_type == Response::Type::Ok ? ChatterType::GoodResponse : ChatterType::ErrorResponse,
		str);

	// did we get a status reponse
	if (response.m_type == Response::Type::Ok && args.size() >= 2 && args[1] == "STATUS")
		readStatusIntoResponse(response);

	return response;
}


//-------------------------------------------------
//  readStatusIntoResponse
//-------------------------------------------------

void MameWorkerController::readStatusIntoResponse(Response &response)
{
	// read the status
	status::update update = readStatus();

	// if we timed out, report that error
	if (m_timedOut)
	{
		response.m_type = Response::Type::Error;
		response.m_text = "Timeout waiting for status update from MAME";
		return;
	}

	// emplace the status update into the response
	response.m_update.emplace(std::move(update));
}


//-------------------------------------------------
//  readStatus - read status XML from a process
//-------------------------------------------------

status::update MameWorkerController::readStatus()
{
	bool done = false;
	QBuffer buffer;

	// open the buffer
	buffer.open(QIODevice::ReadWrite);

	// because XmlParser::parse() is not smart enough to read until XML ends, we are using this
	// crude mechanism to read the XML while leaving everything else intact
	while (!done)
	{
		QString line = reallyReadLineFromProcess();
		buffer.write(line.toUtf8());

		if (line.isEmpty() || line.startsWith("</"))
			done = true;
	}

	// now that we have our own private buffer, read it
	buffer.seek(0);
	return status::update::read(buffer);
}


//-------------------------------------------------
//  reallyReadLineFromProcess - read a line from
//	a QProcess, all the while attempting to accomodate
//	the behavior of QProcess
//-------------------------------------------------

QString MameWorkerController::reallyReadLineFromProcess()
{
	QDateTime readStartDateTime = QDateTime::currentDateTime();
	QDateTime timeoutDateTime = readStartDateTime.addSecs(TIMEOUT_SECONDS);

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
	while (result.isEmpty() && !m_timedOut && m_process.state() == QProcess::ProcessState::Running)
	{
		if (!m_process.canReadLine())
		{
			// yes Qt, we _really_ want to block!  whole heartedly!  but at the same time we
			// don't want to block forever without any escape
			m_process.waitForReadyRead(50);
		}

		// read a line if we can
		if (m_process.canReadLine())
		{
			result = m_process.readLine();
		}
		else if (QDateTime::currentDateTime() >= timeoutDateTime)
		{
			// bummer, we've timed out!  we consider this to be irrecoverable
			m_timedOut = true;
		}
	}
	return result;
}


//-------------------------------------------------
//  issueCommand
//-------------------------------------------------

void MameWorkerController::issueCommand(const QString &command)
{
	if (LOG_COMMANDS)
		qDebug("MameWorkerController::issueCommand(): command='%s'", command.trimmed().toStdString().c_str());
	callChatterCallback(ChatterType::Command, command);

	m_process.write(command.toUtf8());
}


//-------------------------------------------------
//  callChatterCallback
//-------------------------------------------------

void MameWorkerController::callChatterCallback(ChatterType chatterType, const QString &text) const
{
	if (m_chatterCallback)
		m_chatterCallback(chatterType, text);
}


//-------------------------------------------------
//  scrapeMameStartupError - used when we do not get
//	an error message from the LUA worker_ui plug-in
//-------------------------------------------------

QString MameWorkerController::scrapeMameStartupError()
{
	// capture MAME's standard output and present it (not ideal, but better than nothing)
	QByteArray errorOutput = m_process.readAllStandardError();
	return errorOutput.length() > 0
		? QString("Error starting MAME:\r\n\r\n%1").arg(QString::fromUtf8(errorOutput))
		: QString("Error starting MAME");
}
