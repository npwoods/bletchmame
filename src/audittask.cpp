/***************************************************************************

	audittask.cpp

	Task for auditing media

***************************************************************************/

#include <QCoreApplication>

#include "audittask.h"
#include "prefs.h"

QEvent::Type AuditResultEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();
QEvent::Type AuditProgressEvent::s_eventId = (QEvent::Type)QEvent::registerEventType();

//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

class AuditTask::Callback : public Audit::ICallback
{
public:
	Callback(AuditTask &host, const PostEventFunc &postEventFunc);

	// virtuals
	virtual bool reportProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total) override final;
	virtual void reportVerdict(int entryIndex, const Audit::Verdict &verdict) override final;

private:
	AuditTask &				m_host;
	const PostEventFunc &	m_postEventFunc;

	void postProgressEvent(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total, std::optional<Audit::Verdict> &&verdict = { });
};


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditTask::AuditTask(bool reportProgress, int cookie)
	: m_cookie(cookie)
{
	using namespace std::chrono_literals;

	// if we're reporting progress, only then set up the throttler
	if (reportProgress)
		m_reportThrottler.emplace(500ms);
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
		SoftwareAuditIdentifier(software.parent().name(), software.name()));
	entry.m_audit.addMediaForSoftware(prefs, software);
	return entry.m_audit;
}


//-------------------------------------------------
//  getIdentifiers
//-------------------------------------------------

std::vector<AuditIdentifier> AuditTask::getIdentifiers() const
{
	std::vector<AuditIdentifier> result;
	for (const Entry &entry : m_entries)
		result.push_back(entry.m_identifier);
	return result;
}


//-------------------------------------------------
//  process
//-------------------------------------------------

void AuditTask::process(const PostEventFunc &postEventFunc)
{
	std::vector<AuditResult> results;

	// prepare a callback
	Callback callback(*this, postEventFunc);

	// run all the audits
	for (const Entry &entry : m_entries)
	{
		// run the audit
		std::optional<AuditStatus> status = entry.m_audit.run(callback);

		// if we didn't get complete results, we've been aborted and bail
		if (!status)
			break;

		// and record the results
		results.emplace_back(AuditIdentifier(entry.m_identifier), status.value());
	}

	// and respond with the event
	auto evt = std::make_unique<AuditResultEvent>(std::move(results), m_cookie);
	postEventFunc(std::move(evt));
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

AuditTask::Entry::Entry(AuditIdentifier &&identifier)
	: m_identifier(std::move(identifier))
{
}



//-------------------------------------------------
//  Callback ctor
//-------------------------------------------------

AuditTask::Callback::Callback(AuditTask &host, const PostEventFunc &postEventFunc)
	: m_host(host)
	, m_postEventFunc(postEventFunc)
{
}


//-------------------------------------------------
//  Callback::reportProgress
//-------------------------------------------------

bool AuditTask::Callback::reportProgress(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t total)
{
	// report progress, if we were asked to do so
	if (m_host.m_reportThrottler.has_value() && m_host.m_reportThrottler->check())
		postProgressEvent(entryIndex, bytesProcessed, total);

	return m_host.hasAborted();
}


//-------------------------------------------------
//  Callback::reportVerdict
//-------------------------------------------------

void AuditTask::Callback::reportVerdict(int entryIndex, const Audit::Verdict &verdict)
{
	// report progress, if we were asked to do so
	if (m_host.m_reportThrottler.has_value())
		postProgressEvent(entryIndex, ~0, ~0, Audit::Verdict(verdict));
}


//-------------------------------------------------
//  Callback::postProgressEvent
//-------------------------------------------------

void AuditTask::Callback::postProgressEvent(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t totalBytes, std::optional<Audit::Verdict> &&verdict)
{
	auto callbackEvt = std::make_unique<AuditProgressEvent>(entryIndex, bytesProcessed, totalBytes, std::move(verdict));
	m_postEventFunc(std::move(callbackEvt));
}


//-------------------------------------------------
//  MachineAuditIdentifier ctor
//-------------------------------------------------

MachineAuditIdentifier::MachineAuditIdentifier(const QString &machineName)
	: m_machineName(machineName)
{
	assert(!machineName.isEmpty());
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
	assert(!m_softwareList.isEmpty());
	assert(!m_software.isEmpty());
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
//  AuditProgressEvent ctor
//-------------------------------------------------

AuditProgressEvent::AuditProgressEvent(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t totalBytes, std::optional<Audit::Verdict> &&verdict)
	: QEvent(eventId())
	, m_entryIndex(entryIndex)
	, m_bytesProcessed(bytesProcessed)
	, m_totalBytes(totalBytes)
	, m_verdict(verdict)
{
}

