/***************************************************************************

    versiontask.cpp

    Task for invoking '-version'

***************************************************************************/

#include <stdexcept>
#include <QCoreApplication>
#include <QThread>

#include "versiontask.h"
#include "task.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> VersionTask
	class VersionTask : public Task
	{
	protected:
		virtual QStringList GetArguments(const Preferences &) const;
		virtual void Process(QProcess &process, QObject &handler) override;
		virtual void Abort() override;
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

QEvent::Type VersionResultEvent::s_eventId = (QEvent::Type) QEvent::registerEventType();

//-------------------------------------------------
//  VersionResultEvent ctor
//-------------------------------------------------

VersionResultEvent::VersionResultEvent(QString &&version)
	: QEvent(eventId())
	, m_version(std::move(version))
{	
}


//-------------------------------------------------
//  GetArguments
//-------------------------------------------------

QStringList VersionTask::GetArguments(const Preferences &) const
{
	return { "-version" };
}


//-------------------------------------------------
//  Abort
//-------------------------------------------------

void VersionTask::Abort()
{
	// do nothing
}


//-------------------------------------------------
//  Process
//-------------------------------------------------

void VersionTask::Process(QProcess &process, QObject &handler)
{
	// get the version
	auto version = QString::fromLocal8Bit(process.readLine());
	auto evt = std::make_unique<VersionResultEvent>(std::move(version));
	QCoreApplication::postEvent(&handler, evt.release());
}


//-------------------------------------------------
//  create_list_xml_task
//-------------------------------------------------

Task::ptr create_version_task()
{
	return std::make_unique<VersionTask>();
}

