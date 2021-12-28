/***************************************************************************

	audittask.h

	Task for auditing media

***************************************************************************/

#ifndef AUDITTASK_H
#define AUDITTASK_H

#include <QEvent>
#include <optional>
#include <variant>

#include "audit.h"
#include "info.h"
#include "task.h"
#include "throttler.h"


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> MachineAuditIdentifier

class MachineAuditIdentifier
{
public:
	// ctor
	MachineAuditIdentifier(const QString &machineName);
	MachineAuditIdentifier(const MachineAuditIdentifier &) = default;
	MachineAuditIdentifier(MachineAuditIdentifier &&) = default;

	// operators
	MachineAuditIdentifier &operator=(const MachineAuditIdentifier &) = default;
	bool operator==(const MachineAuditIdentifier &) const = default;

	// accessors
	const QString &machineName() const { return m_machineName; }

private:
	QString	m_machineName;
};


namespace std
{
	template<>
	struct hash<MachineAuditIdentifier>
	{
		std::size_t operator()(const MachineAuditIdentifier &x) const;
	};
}


// ======================> SoftwareAuditIdentifier

class SoftwareAuditIdentifier
{
public:
	// ctor
	SoftwareAuditIdentifier(const QString &softwareList, const QString &software);
	SoftwareAuditIdentifier(const SoftwareAuditIdentifier &) = default;
	SoftwareAuditIdentifier(SoftwareAuditIdentifier &&) = default;

	// operators
	SoftwareAuditIdentifier &operator=(const SoftwareAuditIdentifier &) = default;
	bool operator==(const SoftwareAuditIdentifier &) const = default;

	// accessors
	const QString &softwareList() const { return m_softwareList; }
	const QString &software() const { return m_software; }

private:
	QString	m_softwareList;
	QString	m_software;
};


namespace std
{
	template<>
	struct hash<SoftwareAuditIdentifier>
	{
		std::size_t operator()(const SoftwareAuditIdentifier &x) const;
	};
}


// ======================> AuditIdentifier

typedef std::variant<MachineAuditIdentifier, SoftwareAuditIdentifier> AuditIdentifier;


// ======================> AuditResult

class AuditResult
{
public:
	AuditResult(AuditIdentifier &&identifier, AuditStatus status);

	// accessors
	const AuditIdentifier &identifier() const { return m_identifier; }
	AuditStatus status() const { return m_status; }

private:
	AuditIdentifier	m_identifier;
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
	std::vector<AuditIdentifier> getIdentifiers() const;

protected:
	// virtuals
	virtual void process(const PostEventFunc &postEventFunc) final override;

private:
	class Callback;

	struct Entry
	{
		Entry(AuditIdentifier &&identifier);

		AuditIdentifier	m_identifier;
		Audit			m_audit;
	};

	std::vector<Entry>			m_entries;
	std::optional<Throttler>	m_reportThrottler;
	int							m_cookie;
};

#endif // AUDITTASK_H
