/***************************************************************************

    versiontask.cpp

    Task for invoking '-version'

***************************************************************************/

// bletchmame headers
#include "versiontask.h"
#include "mametask.h"
#include "utility.h"

// Qt headers
#include <QCoreApplication>
#include <QThread>

// standard headers
#include <stdexcept>


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
		virtual void run(QProcess &process) override final;
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
//  run
//-------------------------------------------------

void VersionTask::run(QProcess &process)
{
	// get the version
	auto version = QString::fromLocal8Bit(process.readLine());

	// and put it on the completion event
	auto evt = std::make_unique<VersionResultEvent>(std::move(version));

	// and post it
	postEventToHost(std::move(evt));
}


//-------------------------------------------------
//  createVersionTask
//-------------------------------------------------

Task::ptr createVersionTask()
{
	return std::make_shared<VersionTask>();
}

