/***************************************************************************

    versiontask.cpp

    Task for invoking '-version'

***************************************************************************/

#include <stdexcept>
#include <QCoreApplication>
#include <QThread>

#include "versiontask.h"
#include "mametask.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	// ======================> VersionTask
	class VersionTask : public MameTask
	{
	protected:
		virtual QStringList getArguments(const Preferences &) const override final;
		virtual void process(QProcess &process, QObject &eventHandler) override final;
	};
}


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
//  process
//-------------------------------------------------

void VersionTask::process(QProcess &process, QObject &eventHandler)
{
	// get the version
	auto version = QString::fromLocal8Bit(process.readLine());

	// and put it on the completion event
	auto evt = std::make_unique<VersionResultEvent>(std::move(version));

	// and post it
	QCoreApplication::postEvent(&eventHandler, evt.release());
}


//-------------------------------------------------
//  createVersionTask
//-------------------------------------------------

Task::ptr createVersionTask()
{
	return std::make_shared<VersionTask>();
}

