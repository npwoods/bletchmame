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

void AuditQueue::push(AuditIdentifier &&identifier)
{
	// has this entry already been pushed?
	auto mapIter = m_auditTaskMap.find(identifier);
	if (mapIter == m_auditTaskMap.end())
	{
		// if not, add it
		m_auditTaskMap.emplace(std::move(identifier), AuditTask::ptr());
	}
	else if (!mapIter->second)
	{
		// if it has but there is no dispatched task, put this at the front of the queue
		auto iter = std::find(m_undispatchedAudits.begin(), m_undispatchedAudits.end(), identifier);
		if (iter != m_undispatchedAudits.end())
			m_undispatchedAudits.erase(iter);
	}
	m_undispatchedAudits.push_front(std::move(identifier));
}


//-------------------------------------------------
//  tryCreateAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::tryCreateAuditTask()
{
	// come up with a list of entries
	std::vector<AuditIdentifier> entries;
	entries.reserve(MAX_AUDITS_PER_TASK);
	while (entries.size() < MAX_AUDITS_PER_TASK && !m_undispatchedAudits.empty())
	{
		// find an undispatched entry
		AuditIdentifier &entry = m_undispatchedAudits.front();

		// add it to our list
		entries.push_back(std::move(entry));

		// and pop it off the queue
		m_undispatchedAudits.pop_front();
	}

	// create a teask for these entries
	AuditTask::ptr result = createAuditTask(entries);

	// only return something if we're not empty
	if (result->isEmpty())
		result.reset();
	return result;
}


//-------------------------------------------------
//  createAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::createAuditTask(const std::vector<AuditIdentifier> &auditIdentifiers)
{
	// create an audit task with a single audit
	AuditTask::ptr auditTask = std::make_shared<AuditTask>(false, currentCookie());

	for (const AuditIdentifier &identifier : auditIdentifiers)
	{
		std::visit([this, &auditTask](auto &&x)
		{
			using T = std::decay_t<decltype(x)>;
			if constexpr (std::is_same_v<T, MachineAuditIdentifier>)
			{
				// machine audit
				std::optional<info::machine> machine = m_infoDb.find_machine(x.machineName());
				if (machine.has_value())
					auditTask->addMachineAudit(m_prefs, *machine);
			}
			else
			{
				throw false;
			}
		}, identifier);
	}
	return auditTask;
}
