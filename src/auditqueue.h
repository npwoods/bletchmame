/***************************************************************************

	auditqueue.h

	Managing a queue for auditing and dispatching tasks

***************************************************************************/

#ifndef AUDITQUEUE_H
#define AUDITQUEUE_H

#include <deque>
#include <unordered_map>

#include "audittask.h"
#include "info.h"
#include "softwarelist.h"


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
	void push(AuditIdentifier &&identifier, bool isPrioritized);
	AuditTask::ptr tryCreateAuditTask();
	void bumpCookie();

private:
	typedef std::unordered_map<AuditIdentifier, AuditTask::ptr> AuditTaskMap;

	const Preferences &					m_prefs;
	const info::database &				m_infoDb;
	const software_list_collection &	m_softwareListCollection;
	int									m_maxAuditsPerTask;
	AuditTaskMap						m_auditTaskMap;
	std::deque<AuditIdentifier>			m_undispatchedAudits;
	int									m_currentCookie;

	// private methods
	std::uint64_t getExpectedMediaSize(const AuditIdentifier &auditIdentifier) const;
	AuditTask::ptr createAuditTask(const std::vector<AuditIdentifier> &auditIdentifiers) const;
	const software_list::software *findSoftware(const QString &softwareList, const QString &software) const;
};

#endif // AUDITQUEUE_H
