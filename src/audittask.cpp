/***************************************************************************

	audittask.cpp

	Task for auditing media

***************************************************************************/

#include <QCoreApplication>

#include "audittask.h"
#include "prefs.h"

QEvent::Type AuditResultEvent::s_eventId;


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  AuditResultEvent ctor
//-------------------------------------------------

AuditResultEvent::AuditResultEvent(std::vector<AuditResult> &&results, int cookie)
	: QEvent(eventId())
	, m_results(std::move(results))
	, m_cookie(cookie)
{
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditTask::AuditTask(int cookie)
	: m_cookie(cookie)
{
}


//-------------------------------------------------
//  addMachineAudit
//-------------------------------------------------

void AuditTask::addMachineAudit(const Preferences &prefs, const info::machine &machine)
{
	Entry &entry = *m_entries.emplace(m_entries.end());
	entry.m_machineName = machine.name();
	entry.m_audit.addMediaForMachine(prefs, machine);
}


//-------------------------------------------------
//  process
//-------------------------------------------------

void AuditTask::process(QObject &eventHandler)
{
	std::vector<AuditResult> results;

	// run all the audits
	for (const Entry &entry : m_entries)
	{
		// run the audit
		AuditStatus status = entry.m_audit.run();

		// and record the results
		results.emplace_back(entry.m_machineName, status);
	}

	// and respond with the event
	auto evt = std::make_unique<AuditResultEvent>(std::move(results), m_cookie);
	QCoreApplication::postEvent(&eventHandler, evt.release());
}
