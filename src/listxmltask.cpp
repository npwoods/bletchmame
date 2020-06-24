/***************************************************************************

    listxmltask.cpp

    Task for invoking '-listxml'

***************************************************************************/

#include <unordered_map>
#include <exception>
#include <QCoreApplication>

#include "listxmltask.h"
#include "xmlparser.h"
#include "utility.h"
#include "info.h"
#include "info_builder.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> ListXmlTask
	class ListXmlTask : public Task
	{
	public:
		ListXmlTask(QString &&output_filename);

	protected:
		virtual QStringList getArguments(const Preferences &) const;
		virtual void process(QProcess &process, QObject &handler) override;
		virtual void abort() override;

	private:
		QString			m_output_filename;
		volatile bool	m_aborted;

		void internalProcess(QProcess &process);
	};

	// ======================> list_xml_exception
	class list_xml_exception : public std::exception
	{
	public:
		list_xml_exception(ListXmlResultEvent::Status status, QString &&message = QString())
			: m_status(status)
			, m_message(message)
		{
		}

		ListXmlResultEvent::Status	m_status;
		QString						m_message;
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

QEvent::Type ListXmlResultEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ListXmlTask::ListXmlTask(QString &&output_filename)
	: m_output_filename(std::move(output_filename))
	, m_aborted(false)
{
}


//-------------------------------------------------
//  getArguments
//-------------------------------------------------

QStringList ListXmlTask::getArguments(const Preferences &) const
{
	return { "-listxml", "-nodtd" };
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void ListXmlTask::abort()
{
	m_aborted = true;
}


//-------------------------------------------------
//  process
//-------------------------------------------------

void ListXmlTask::process(QProcess &process, QObject &handler)
{
	ListXmlResultEvent::Status status;
	QString errorMessage;
	try
	{
		// process
		internalProcess(process);

		// we've succeeded!
		status = ListXmlResultEvent::Status::SUCCESS;
	}
	catch (list_xml_exception &ex)
	{
		// an exception has occurred
		status = ex.m_status;
		errorMessage = std::move(ex.m_message);
	}

	// regardless of what happened, notify the main thread
	auto evt = std::make_unique<ListXmlResultEvent>(status, std::move(errorMessage));
	QCoreApplication::postEvent(&handler, evt.release());
}


//-------------------------------------------------
//  InternalProcess
//-------------------------------------------------

void ListXmlTask::internalProcess(QProcess &process)
{
	info::database_builder builder;

	// first process the XML
	QDataStream input(&process);
	QString error_message;
	bool success = builder.process_xml(input, error_message);

	// before we check to see if there is a parsing error, check for an abort - under which
	// scenario a parsing error is expected
	if (m_aborted)
		throw list_xml_exception(ListXmlResultEvent::Status::ABORTED);

	// now check for a parse error (which should be very unlikely)
	if (!success)
		throw list_xml_exception(ListXmlResultEvent::Status::ERROR, QString("Error parsing XML from MAME -listxml: %1").arg(error_message));

	// we finally have all of the info accumulated; now we can get to business with writing
	// to the actual file
	QFile file(m_output_filename);
	QDataStream output(&file);
	if (output.status() != QDataStream::Status::Ok)
		throw list_xml_exception(ListXmlResultEvent::Status::ERROR, QString("Could not open file: %1").arg(m_output_filename));

	// emit the data
	builder.emit_info(output);
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
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_list_xml_task(QString &&dest)
{
	return std::make_shared<ListXmlTask>(std::move(dest));
}
