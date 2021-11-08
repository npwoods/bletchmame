/***************************************************************************

	auditcursor.h

	Maintains a cursor through auditables

***************************************************************************/

#ifndef AUDITCURSOR_H
#define AUDITCURSOR_H

#include <optional>

#include "audittask.h"
#include "info.h"

class Preferences;

// ======================> AuditCursor

class AuditCursor
{
public:
	// ctor
	AuditCursor();

	// accessors
	int currentPosition() const { assert(m_position >= 0); return m_position; }
	bool isComplete() const { return m_position < 0; }

	// methods
	std::optional<AuditIdentifier> next(int basePosition);

protected:
	// abstract methods
	virtual int getAuditableCount() const = 0;
	virtual std::optional<AuditIdentifier> getIdentifier(int position) const = 0;

private:
	int		m_position;
};


// ======================> MachineAuditCursor

class MachineAuditCursor: public AuditCursor
{
public:
	// ctor
	MachineAuditCursor(const Preferences &prefs, const info::database &infoDb);

protected:
	// abstract methods
	virtual int getAuditableCount() const override final;
	virtual std::optional<AuditIdentifier> getIdentifier(int position) const override final;

private:
	const Preferences &		m_prefs;
	const info::database &	m_infoDb;
};


#endif // AUDITCURSOR_H
