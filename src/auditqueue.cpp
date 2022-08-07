/***************************************************************************

	auditqueue.cpp

	Managing a queue for auditing and dispatching tasks

***************************************************************************/

// bletchmame headers
#include "auditqueue.h"


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditQueue::AuditQueue(const Preferences &prefs, const info::database &infoDb, const software_list_collection &softwareListCollection, int maxAuditsPerTask)
	: m_prefs(prefs)
	, m_infoDb(infoDb)
	, m_softwareListCollection(softwareListCollection)
	, m_maxAuditsPerTask(maxAuditsPerTask)
	, m_currentCookie(100)
{
}


//-------------------------------------------------
//  push
//-------------------------------------------------

void AuditQueue::push(Identifier &&identifier, bool isPrioritized)
{
	// has this entry already been pushed?
	auto mapIter = m_auditTaskMap.find(identifier);
	if (mapIter == m_auditTaskMap.end())
	{
		// if not, add it
		mapIter = m_auditTaskMap.emplace(identifier, AuditTask::ptr()).first;
	}

	// at this point, the identifier is definitely in the task map; we have to
	// check to see if there is a task and if so, we can leave it alone
	if (!mapIter->second)
	{
		// there is no task; try to find it in the undispatched audit queue
		auto iter = std::find(m_undispatchedAudits.begin(), m_undispatchedAudits.end(), identifier);

		if (isPrioritized)
		{
			// if this has been prioritized, remove it if present and put it at the front
			if (iter != m_undispatchedAudits.end())
				m_undispatchedAudits.erase(iter);
			m_undispatchedAudits.push_front(std::move(identifier));
		}
		else
		{
			// for non prioritized audits, add to the back if it is not there, leave alone otherwise
			if (iter == m_undispatchedAudits.end())
				m_undispatchedAudits.push_back(std::move(identifier));
		}
	}
}


//-------------------------------------------------
//  tryCreateAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::tryCreateAuditTask()
{
	// we want a rough maximum media size for any given task (though
	// we will tolerate a single media above that size)
	const std::uint64_t MAX_MEDIA_SIZE_PER_TASK = 50000000;

	// prepare a vector of entries
	std::vector<Identifier> entries;
	entries.reserve(m_maxAuditsPerTask);
	std::uint64_t totalMediaSize = 0;

	// loop until that vector is populated
	while (!m_undispatchedAudits.empty()				// are there undispatched audits for us?
		&& entries.size() < m_maxAuditsPerTask			// did we hit the limit of individual audits?
		&& totalMediaSize < MAX_MEDIA_SIZE_PER_TASK)	// and finally did we hit the maximum media size?
	{
		// find an undispatched entry
		Identifier &entry = m_undispatchedAudits.front();

		// estimate its size and bail if it would be too big
		std::uint64_t mediaSize = getExpectedMediaSize(entry);
		if (!entries.empty() && totalMediaSize + mediaSize >= MAX_MEDIA_SIZE_PER_TASK)
			break;

		// add it to our list
		entries.push_back(std::move(entry));
		totalMediaSize += mediaSize;

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
//  getExpectedMediaSize
//-------------------------------------------------

std::uint64_t AuditQueue::getExpectedMediaSize(const Identifier &auditIdentifier) const
{
	std::uint64_t result = 0;
	std::visit(util::overloaded
	{
		[this, &result] (const MachineIdentifier &x)
		{
			// machine audit
			std::optional<info::machine> machine = m_infoDb.find_machine(x.machineName());
			if (machine)
			{
				for (info::rom rom : machine->roms())
					result += rom.size();
			}
		},
		[this, &result](const SoftwareIdentifier &x)
		{
			// software audit
			const software_list::software *software = findSoftware(x.softwareList(), x.software());
			if (software)
			{
				for (const software_list::part &part : software->parts())
				{
					for (const software_list::dataarea &area : part.dataareas())
					{
						for (const software_list::rom &rom : area.roms())
						{
							if (rom.size())
								result += *rom.size();
						}
					}
				}
			}
		}
	}, auditIdentifier);
	return result;
}


//-------------------------------------------------
//  createAuditTask
//-------------------------------------------------

AuditTask::ptr AuditQueue::createAuditTask(const std::vector<Identifier> &auditIdentifiers) const
{
	// create an audit task with a single audit
	AuditTask::ptr auditTask = std::make_shared<AuditTask>(false, currentCookie());

	for (const Identifier &identifier : auditIdentifiers)
	{
		std::visit(util::overloaded
		{
			[this, &auditTask](const MachineIdentifier &x)
			{
				// machine audit
				std::optional<info::machine> machine = m_infoDb.find_machine(x.machineName());
				if (machine)
					auditTask->addMachineAudit(m_prefs, *machine);
			},
			[this, &auditTask](const SoftwareIdentifier &x)
			{
				// software audit
				const software_list::software *software = findSoftware(x.softwareList(), x.software());
				if (software)
					auditTask->addSoftwareAudit(m_prefs, *software);
			}
		}, identifier);
	}
	return auditTask;
}


//-------------------------------------------------
//  findSoftware
//-------------------------------------------------

const software_list::software *AuditQueue::findSoftware(std::u8string_view softwareList, std::u8string_view software) const
{
	// find the software list with the specified name
	QString softwareListQString = util::toQString(softwareList);
	auto softwareListIter = std::ranges::find_if(m_softwareListCollection.software_lists(), [&softwareListQString](const software_list::ptr &ptr)
	{
		return ptr->name() == softwareListQString;
	});
	if (softwareListIter == m_softwareListCollection.software_lists().end())
		return nullptr;

	// find the software with the specified name
	QString softwareQString = util::toQString(software);
	auto softwareIter = std::ranges::find_if((*softwareListIter)->get_software(), [&softwareQString](const software_list::software &sw)
	{
		return sw.name() == softwareQString;
	});
	if (softwareIter == (*softwareListIter)->get_software().end())
		return nullptr;

	// we've succeeded!
	return &*softwareIter;
}


//-------------------------------------------------
//  bumpCookie
//-------------------------------------------------

void AuditQueue::bumpCookie()
{
	m_currentCookie++;
}

