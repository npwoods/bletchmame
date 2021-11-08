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
	AuditQueue(const Preferences &prefs, const info::database &infoDb, const software_list_collection &softwareListCollection);

	// accessors
	bool hasUndispatched() const { return m_undispatchedAudits.size() > 0; }
	bool isCloseToEmpty() const { return m_undispatchedAudits.size() < MAX_AUDITS_PER_TASK; }
	int currentCookie() const { return m_currentCookie; }

	// methods
	void push(AuditIdentifier &&identifier, bool isPrioritized);
	AuditTask::ptr tryCreateAuditTask();

private:
	const int MAX_AUDITS_PER_TASK = 3;

	typedef std::unordered_map<AuditIdentifier, AuditTask::ptr> AuditTaskMap;

	const Preferences &					m_prefs;
	const info::database &				m_infoDb;
	const software_list_collection &	m_softwareListCollection;
	AuditTaskMap						m_auditTaskMap;
	std::deque<AuditIdentifier>			m_undispatchedAudits;
	int									m_currentCookie;

	// private methods
	AuditTask::ptr createAuditTask(const std::vector<AuditIdentifier> &auditIdentifiers);
	const software_list::software *findSoftware(const QString &softwareList, const QString &software) const;
};

#endif // AUDITQUEUE_H
