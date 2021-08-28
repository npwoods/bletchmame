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
	bool hasUndispatched() const { return m_undispatchedEntries.size() > 0; }
	int currentCookie() const { return m_currentCookie; }

	// methods
	void push(const info::machine &machine);
	AuditTask::ptr tryCreateAuditTask();

private:
	class Entry
	{
	public:
		Entry(const QString &machineName, const QString &softwareName = "");
		Entry(const Entry &) = default;
		Entry(Entry &&) = default;
		Entry &operator=(const Entry &) = default;
		bool operator==(const Entry &) const = default;

		// accessors
		const QString &machineName() const { return m_machineName; }
		const QString &softwareName() const { return m_softwareName; }

	private:
		QString m_machineName;
		QString m_softwareName;
	};

	struct EntryMapHash
	{
		std::size_t operator()(const Entry &x) const noexcept;
	};

	struct EntryMapEquals
	{
		bool operator()(const Entry &lhs, const Entry &rhs) const;
	};

	const Preferences &														m_prefs;
	const info::database &													m_infoDb;
	std::unordered_map<Entry, AuditTask::ptr, EntryMapHash, EntryMapEquals>	m_entryTaskMap;
	std::deque<Entry>														m_undispatchedEntries;
	int																		m_currentCookie;

	AuditTask::ptr createAuditTask(const std::vector<Entry> &entries);
};

#endif // AUDITQUEUE_H
