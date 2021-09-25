/***************************************************************************

	audittask.h

	Task for auditing media

***************************************************************************/

#ifndef AUDITTASK_H
#define AUDITTASK_H

#include <QEvent>

#include "audit.h"
#include "info.h"
#include "task.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> AuditResult

class AuditResult
{
public:
	AuditResult(const QString &machineName, AuditStatus status)
		: m_machineName(machineName)
		, m_status(status)
	{
	}

	// accessors
	const QString &machineName() const { return m_machineName; }
	AuditStatus status() const { return m_status; }

private:
	QString		m_machineName;
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


// ======================> AuditSingleMediaEvent

class AuditSingleMediaEvent : public QEvent
{
public:
	AuditSingleMediaEvent(int entryIndex, Audit::Verdict verdict);
	static QEvent::Type eventId() { return s_eventId; }

	// accessors
	int entryIndex() const { return m_entryIndex; }
	const Audit::Verdict &verdict() const { return m_verdict; }

private:
	static QEvent::Type	s_eventId;
	int					m_entryIndex;
	Audit::Verdict		m_verdict;
};


// ======================> AuditTask

class AuditTask : public Task
{
public:
	typedef std::shared_ptr<AuditTask> ptr;

	// ctor
	AuditTask(bool reportSingleMedia, int cookie);

	// methods
	const Audit &addMachineAudit(const Preferences &prefs, const info::machine &machine);

protected:
	// virtuals
	virtual void process(QObject &eventHandler) final override;

private:
	struct Entry
	{
		QString	m_machineName;
		Audit	m_audit;
	};

	std::vector<Entry>	m_entries;
	bool				m_reportSingleMedia;
	int					m_cookie;
};

#endif // AUDITTASK_H
