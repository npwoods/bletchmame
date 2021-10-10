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


// ======================> AuditQueue

class AuditQueue
{
public:
	// ctor
	AuditQueue(const Preferences &prefs, const info::database &infoDb);

	// accessors
	bool hasUndispatched() const { return m_undispatchedAudits.size() > 0; }
	int currentCookie() const { return m_currentCookie; }

	// methods
	void push(AuditIdentifier &&identifier);
	AuditTask::ptr tryCreateAuditTask();

private:
	typedef std::unordered_map<AuditIdentifier, AuditTask::ptr> AuditTaskMap;

	const Preferences &					m_prefs;
	const info::database &				m_infoDb;
	AuditTaskMap						m_auditTaskMap;
	std::deque<AuditIdentifier>			m_undispatchedAudits;
	int									m_currentCookie;

	// private methods
	AuditTask::ptr createAuditTask(const std::vector<AuditIdentifier> &auditIdentifiers);
};

#endif // AUDITQUEUE_H
