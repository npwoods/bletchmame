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
	Entry &entry = *m_entries.emplace(
		m_entries.end(),
		MachineAuditIdentifier(machine.name()));
	entry.m_audit.addMediaForMachine(prefs, machine);
	return entry.m_audit;
}


//-------------------------------------------------
//  addSoftwareAudit
//-------------------------------------------------

const Audit &AuditTask::addSoftwareAudit(const Preferences &prefs, const software_list::software &software)
{
	Entry &entry = *m_entries.emplace(
		m_entries.end(),
		SoftwareAuditIdentifier(software.software_list().name(), software.name()));
	entry.m_audit.addMediaForSoftware(prefs, software);
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
		results.emplace_back(AuditIdentifier(entry.m_identifier), status);
	}

	// and respond with the event
	auto evt = std::make_unique<AuditResultEvent>(std::move(results), m_cookie);
	QCoreApplication::postEvent(&eventHandler, evt.release());
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

AuditTask::Entry::Entry(AuditIdentifier &&identifier)
	: m_identifier(std::move(identifier))
{
}


//-------------------------------------------------
//  MachineAuditIdentifier ctor
//-------------------------------------------------

MachineAuditIdentifier::MachineAuditIdentifier(const QString &machineName)
	: m_machineName(machineName)
{
}


//-------------------------------------------------
//  std::hash<MachineAuditIdentifier>::operator()
//-------------------------------------------------

std::size_t std::hash<MachineAuditIdentifier>::operator()(const MachineAuditIdentifier &identifier) const
{
	return std::hash<QString>()(identifier.machineName());
}


//-------------------------------------------------
//  SoftwareAuditIdentifier ctor
//-------------------------------------------------

SoftwareAuditIdentifier::SoftwareAuditIdentifier(const QString &softwareList, const QString &software)
	: m_softwareList(softwareList)
	, m_software(software)
{
}


//-------------------------------------------------
//  std::hash<SoftwareAuditIdentifier>::operator()
//-------------------------------------------------

std::size_t std::hash<SoftwareAuditIdentifier>::operator()(const SoftwareAuditIdentifier &identifier) const
{
	return std::hash<QString>()(identifier.softwareList())
		^ std::hash<QString>()(identifier.software());
}


//-------------------------------------------------
//  AuditResult ctor
//-------------------------------------------------

AuditResult::AuditResult(AuditIdentifier &&identifier, AuditStatus status)
	: m_identifier(std::move(identifier))
	, m_status(status)
{
}


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

