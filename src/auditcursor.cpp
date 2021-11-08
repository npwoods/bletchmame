/***************************************************************************

	auditcursor.cpp

	Maintains a cursor through auditables

***************************************************************************/

#include "auditcursor.h"


//-------------------------------------------------
//  AuditCursor ctor
//-------------------------------------------------

AuditCursor::AuditCursor()
	: m_position(0)
{
}


//-------------------------------------------------
//  AuditCursor::next
//-------------------------------------------------

std::optional<AuditIdentifier> AuditCursor::next(int basePosition)
{
	// sanity checks
	assert(basePosition >= 0);

	// work out the basics
	int auditableCount = getAuditableCount();
	basePosition %= auditableCount;

	// try to find the next auditable
	std::optional<AuditIdentifier> result;
	while (!result.has_value() && (m_position = (m_position + 1) % auditableCount) != basePosition)
		result = getIdentifier(m_position);

	// if we didn't find anything, mark ourselves complete
	if (!result)
		m_position = -1;

	// we're done - maybe we have one, maybe we don't
	return result;
}


//-------------------------------------------------
//  MachineAuditCursor ctor
//-------------------------------------------------

MachineAuditCursor::MachineAuditCursor(const Preferences &prefs, const info::database &infoDb)
	: m_prefs(prefs)
	, m_infoDb(infoDb)
{
}


//-------------------------------------------------
//  MachineAuditCursor::getAuditableCount
//-------------------------------------------------

int MachineAuditCursor::getAuditableCount() const
{
	return util::safe_static_cast<int>(m_infoDb.machines().size());
}


//-------------------------------------------------
//  MachineAuditCursor::getIdentifier
//-------------------------------------------------

std::optional<AuditIdentifier> MachineAuditCursor::getIdentifier(int position) const
{
	const QString &machineName = m_infoDb.machines()[position].name();
	return m_prefs.getMachineAuditStatus(machineName) == AuditStatus::Unknown
		? MachineAuditIdentifier(machineName)
		: std::optional<AuditIdentifier>();
}
