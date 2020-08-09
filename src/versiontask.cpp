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
		virtual QStringList getArguments(const Preferences &) const;
		virtual void process(QProcess &process, QObject &handler) override;
		virtual void abort() override;
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
//  getArguments
//-------------------------------------------------

QStringList VersionTask::getArguments(const Preferences &) const
{
	return { "-version" };
}


//-------------------------------------------------
//  abort
//-------------------------------------------------

void VersionTask::abort()
{
	// do nothing
}


//-------------------------------------------------
//  process
//-------------------------------------------------

void VersionTask::process(QProcess &process, QObject &handler)
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
	return std::make_shared<VersionTask>();
}

