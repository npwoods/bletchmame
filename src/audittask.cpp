/***************************************************************************

	audittask.cpp

	Task for auditing media

***************************************************************************/

#include <QCoreApplication>

#include "audittask.h"
#include "prefs.h"

QEvent::Type AuditResultEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();
QEvent::Type AuditSingleMediaEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();


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
//  AuditSingleMediaEvent ctor
//-------------------------------------------------

AuditSingleMediaEvent::AuditSingleMediaEvent(int entryIndex, Audit::Verdict verdict)
	: QEvent(eventId())
	, m_entryIndex(entryIndex)
	, m_verdict(verdict)
{
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditTask::AuditTask(bool reportSingleMedia, int cookie)
	: m_reportSingleMedia(reportSingleMedia)
	, m_cookie(cookie)
{
}


//-------------------------------------------------
//  addMachineAudit
//-------------------------------------------------

const Audit &AuditTask::addMachineAudit(const Preferences &prefs, const info::machine &machine)
{
	Entry &entry = *m_entries.emplace(m_entries.end());
	entry.m_machineName = machine.name();
	entry.m_audit.addMediaForMachine(prefs, machine);
	return entry.m_audit;
}


//-------------------------------------------------
//  process
//-------------------------------------------------

void AuditTask::process(QObject &eventHandler)
{
	std::vector<AuditResult> results;

	// prepare a callback
	Audit::Callback callback;
	if (m_reportSingleMedia)
	{
		callback = [&eventHandler](int entryIndex, const Audit::Verdict &verdict)
		{
			auto callbackEvt = std::make_unique<AuditSingleMediaEvent>(entryIndex, verdict);
			QCoreApplication::postEvent(&eventHandler, callbackEvt.release());
		};
	}

	// run all the audits
	for (const Entry &entry : m_entries)
	{
		// run the audit
		AuditStatus status = entry.m_audit.run(callback);

		// and record the results
		results.emplace_back(entry.m_machineName, status);
	}

	// and respond with the event
	auto evt = std::make_unique<AuditResultEvent>(std::move(results), m_cookie);
	QCoreApplication::postEvent(&eventHandler, evt.release());
}
