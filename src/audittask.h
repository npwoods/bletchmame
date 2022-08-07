/***************************************************************************

	audittask.h

	Task for auditing media

***************************************************************************/

#ifndef AUDITTASK_H
#define AUDITTASK_H

#include <QEvent>
#include <optional>

#include "audit.h"
#include "identifier.h"
#include "info.h"
#include "task.h"
#include "throttler.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> AuditResult

class AuditResult
{
public:
	AuditResult(Identifier &&identifier, AuditStatus status);

	// accessors
	const Identifier &identifier() const { return m_identifier; }
	AuditStatus status() const { return m_status; }

private:
	Identifier	m_identifier;
	AuditStatus	m_status;
};


// ======================> AuditResultEvent

class AuditResultEvent : public QEvent
{
public:
	AuditResultEvent(std::vector<AuditResult> &&results, int cookie);
	static QEvent::Type eventId() { return s_eventId; }

	// accessors
	const std::vector<AuditResult> &results() const { return m_results; }
	int cookie() const { return m_cookie; }

private:
	static QEvent::Type			s_eventId;
	std::vector<AuditResult>	m_results;
	int							m_cookie;
};


// ======================> AuditProgressEvent

class AuditProgressEvent : public QEvent
{
public:
	AuditProgressEvent(int entryIndex, std::uint64_t bytesProcessed, std::uint64_t totalBytes, std::optional<Audit::Verdict> &&verdict);
	static QEvent::Type eventId() { return s_eventId; }

	// accessors
	int entryIndex() const { return m_entryIndex; }
	std::uint64_t bytesProcessed() const { return m_bytesProcessed; }
	std::uint64_t totalBytes() const { return m_totalBytes; }
	const std::optional<Audit::Verdict> &verdict() const { return m_verdict; }

private:
	static QEvent::Type				s_eventId;
	int								m_entryIndex;
	std::uint64_t					m_bytesProcessed;
	std::uint64_t					m_totalBytes;
	std::optional<Audit::Verdict>	m_verdict;
};


// ======================> AuditTask

class AuditTask : public Task
{
public:
	class Test;

	typedef std::shared_ptr<AuditTask> ptr;

	// ctor
	AuditTask(bool reportProgress, int cookie);

	// methods
	const Audit &addMachineAudit(const Preferences &prefs, const info::machine &machine);
	const Audit &addSoftwareAudit(const Preferences &prefs, const software_list::software &software);

	// accessors
	bool isEmpty() const { return m_entries.empty(); }
	std::vector<Identifier> getIdentifiers() const;

protected:
	// virtuals
	virtual void run() final override;

private:
	class Callback;

	struct Entry
	{
		Entry(Identifier &&identifier);

		Identifier		m_identifier;
		Audit			m_audit;
	};

	std::vector<Entry>			m_entries;
	std::optional<Throttler>	m_reportThrottler;
	int							m_cookie;
};

#endif // AUDITTASK_H
