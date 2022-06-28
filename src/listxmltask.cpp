/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

// bletchmame headers
#include "listxmltask.h"
#include "perfprofiler.h"
#include "xmlparser.h"
#include "utility.h"
#include "info.h"
#include "info_builder.h"

// Qt headers
#include <QCoreApplication>
#include <QDir>

// standard headers
#include <unordered_map>
#include <exception>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

QEvent::Type ListXmlProgressEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();
QEvent::Type ListXmlResultEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ListXmlTask::ListXmlTask(QString &&outputFilename)
	: m_outputFilename(std::move(outputFilename))
{
}


//-------------------------------------------------
//  getArguments
//-------------------------------------------------

QStringList ListXmlTask::getArguments(const Preferences &) const
{
	return { "-listxml" };
}


//-------------------------------------------------
//  run
//-------------------------------------------------

void ListXmlTask::run(std::optional<QProcess> &process)
{
	// profile this
	PerformanceProfiler perfProfiler("listxml.profiledata.txt");
	ProfilerScope prof(CURRENT_FUNCTION);

	// callback
	auto progressCallback = [this](int count, std::u8string_view machineName, std::u8string_view machineDescription)
	{
		auto evt = std::make_unique<ListXmlProgressEvent>(count, util::toQString(machineName), util::toQString(machineDescription));
		postEventToHost(std::move(evt));
	};

	ListXmlResultEvent::Status status;
	QString errorMessage;
	if (process)
	{
		// process
		std::optional<ListXmlError> result = internalRun(*process, progressCallback);
		if (result)
		{
			// an exception has occurred
			status = result->status();
			errorMessage = result->message();
		}
		else
		{
			// we've succeeded!
			status = ListXmlResultEvent::Status::SUCCESS;
		}
	}
	else
	{
		status = ListXmlResultEvent::Status::ERROR;
		errorMessage = "Could not invoke MAME";
	}

	// regardless of what happened, notify the main thread
	auto evt = std::make_unique<ListXmlResultEvent>(status, std::move(errorMessage));
	postEventToHost(std::move(evt));
}


//-------------------------------------------------
//  internalRun
//-------------------------------------------------

std::optional<ListXmlTask::ListXmlError> ListXmlTask::internalRun(QIODevice &process, const info::database_builder::ProcessXmlCallback &progressCallback)
{
	info::database_builder builder;

	// first process the XML
	QString error_message;
	bool success = builder.process_xml(process, error_message, progressCallback);

	// before we check to see if there is a parsing error, check for an abort - under which
	// scenario a parsing error is expected
	if (isInterruptionRequested())
		return ListXmlError(ListXmlResultEvent::Status::ABORTED);

	// now check for a parse error (which should be very unlikely)
	if (!success)
		return ListXmlError(ListXmlResultEvent::Status::ERROR, QString("Error parsing XML from MAME -listxml: %1").arg(error_message));

	// try creating the directory if its not present
	QDir dir = QFileInfo(m_outputFilename).dir();
	if (!dir.exists())
		QDir().mkpath(dir.absolutePath());

	// we finally have all of the info accumulated; now we can get to business with writing
	// to the actual file
	QFile file(m_outputFilename);
	if (!file.open(QIODevice::WriteOnly))
		return ListXmlError(ListXmlResultEvent::Status::ERROR, QString("Could not open file: %1").arg(m_outputFilename));

	// emit the data and return
	builder.emit_info(file);
	return { };
}


//-------------------------------------------------
//  ListXmlProgressEvent ctor
//-------------------------------------------------

ListXmlProgressEvent::ListXmlProgressEvent(int machineCount, QString &&machineName, QString &&machineDescription)
	: QEvent(eventId())
	, m_machineCount(machineCount)
	, m_machineName(std::move(machineName))
	, m_machineDescription(std::move(machineDescription))
{
}



//-------------------------------------------------
//  ListXmlResultEvent ctor
//-------------------------------------------------

ListXmlResultEvent::ListXmlResultEvent(Status status, QString &&errorMessage)
	: QEvent(eventId())
	, m_status(status)
	, m_errorMessage(errorMessage)
{
}


//-------------------------------------------------
//  ListXmlError ctor
//-------------------------------------------------

ListXmlTask::ListXmlError::ListXmlError(ListXmlResultEvent::Status status, QString &&message)
	: m_status(status)
	, m_message(std::move(message))
{
}


//-------------------------------------------------
//  ListXmlError::status
//-------------------------------------------------

ListXmlResultEvent::Status ListXmlTask::ListXmlError::status() const
{
	return m_status;
}


//-------------------------------------------------
//  ListXmlError::message
//-------------------------------------------------

QString &ListXmlTask::ListXmlError::message()
{
	return m_message;
}

