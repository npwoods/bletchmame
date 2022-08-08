/***************************************************************************

	auditqueue.h

	Managing a queue for auditing and dispatching tasks

***************************************************************************/

#ifndef AUDITQUEUE_H
#define AUDITQUEUE_H

// bletchmame headers
#include "audittask.h"
#include "info.h"
#include "softwarelist.h"

// standard headers
#include <deque>
#include <unordered_map>


// ======================> AuditQueue

class AuditQueue
{
public:
	class Test;

	// ctor
	AuditQueue(const Preferences &prefs, const info::database &infoDb, const software_list_collection &softwareListCollection, int maxAuditsPerTask);

	// accessors
	bool hasUndispatched() const { return m_undispatchedAudits.size() > 0; }
	bool isCloseToEmpty() const { return m_undispatchedAudits.size() < m_maxAuditsPerTask; }
	int currentCookie() const { return m_currentCookie; }

	// methods
	void push(Identifier &&identifier, bool isPrioritized);
	AuditTask::ptr tryCreateAuditTask();
	void bumpCookie();

private:
	typedef std::unordered_map<Identifier, AuditTask::ptr> AuditTaskMap;

	const Preferences &					m_prefs;
	const info::database &				m_infoDb;
	const software_list_collection &	m_softwareListCollection;
	int									m_maxAuditsPerTask;
	AuditTaskMap						m_auditTaskMap;
	std::deque<Identifier>				m_undispatchedAudits;
	int									m_currentCookie;

	// private methods
	std::uint64_t getExpectedMediaSize(const Identifier &auditIdentifier) const;
	AuditTask::ptr createAuditTask(const std::vector<Identifier> &auditIdentifiers) const;
	const software_list::software *findSoftware(std::u8string_view softwareList, std::u8string_view software) const;
};

#endif // AUDITQUEUE_H
