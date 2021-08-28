/***************************************************************************

	auditqueue.cpp

	Managing a queue for auditing and dispatching tasks

***************************************************************************/

#include "auditqueue.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define MAX_AUDITS_PER_TASK		3


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditQueue::AuditQueue(const Preferences &prefs, const info::database &infoDb)
	: m_prefs(prefs)
	, m_infoDb(infoDb)
	, m_currentCookie(100)
{
}


//-------------------------------------------------
//  push
//-------------------------------------------------

void AuditQueue::push(const info::machine &machine)
{
	// create an entry
	Entry entry(machine.name());

	// has this entry already been pushed?
	auto mapIter = m_entryTaskMap.find(entry);
	if (mapIter == m_entryTaskMap.end())
	{
		// if not, add it
		m_entryTaskMap.emplace(entry, AuditTask::ptr());
	}
	else if (!mapIter->second)
	{
		// if it has but there is no dispatched task, put this at the front of the queue
		auto iter = std::find(m_undispatchedEntries.begin(), m_undispatchedEntries.end(), entry);
		if (iter != m_undispatchedEntries.end())
			m_undispatchedEntries.erase(iter);
	}
	m_undispatchedEntries.push_front(std::move(entry));
}


//-------------------------------------------------
//  tryCreateAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::tryCreateAuditTask()
{
	// come up with a list of entries
	std::vector<Entry> entries;
	entries.reserve(MAX_AUDITS_PER_TASK);
	while (entries.size() < MAX_AUDITS_PER_TASK && !m_undispatchedEntries.empty())
	{
		// find an undispatched entry
		Entry &entry = m_undispatchedEntries.front();

		// add it to our list
		entries.push_back(std::move(entry));

		// and pop it off the queue
		m_undispatchedEntries.pop_front();
	}

	// if we found any, create a task
	AuditTask::ptr result;
	if (entries.size() > 0)
		result = createAuditTask(entries);
	return result;
}


//-------------------------------------------------
//  createAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::createAuditTask(const std::vector<Entry> &entries)
{
	// create an audit task with a single audit
	AuditTask::ptr auditTask = std::make_shared<AuditTask>(currentCookie());

	for (const Entry &entry : entries)
	{
		// what sort of audit is this?
		if (!entry.machineName().isEmpty() && entry.softwareName().isEmpty())
		{
			// machine audit
			info::machine machine = *m_infoDb.find_machine(entry.machineName());
			auditTask->addMachineAudit(m_prefs, machine);
		}
		else
		{
			// should not get here
			throw false;
		}
	}
	return auditTask;
}


//-------------------------------------------------
//  Entry ctor
//-------------------------------------------------

AuditQueue::Entry::Entry(const QString &machineName, const QString &softwareName)
	: m_machineName(machineName)
	, m_softwareName(softwareName)
{
}


//-------------------------------------------------
//  EntryMapHash::operator()
//-------------------------------------------------

std::size_t AuditQueue::EntryMapHash::operator()(const AuditQueue::Entry &x) const noexcept
{
	return std::hash<QString>()(x.machineName())
		^ std::hash<QString>()(x.softwareName());
}


//-------------------------------------------------
//  EntryMapEquals::operator()
//-------------------------------------------------

bool AuditQueue::EntryMapEquals::operator()(const Entry &lhs, const Entry &rhs) const
{
	return lhs == rhs;
}
