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
//  AuditCursor::awaken
//-------------------------------------------------

void AuditCursor::awaken()
{
	if (m_position < 0)
		m_position = 0;
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
//  MachineAuditCursor::setMachineFilter
//-------------------------------------------------

void MachineAuditCursor::setMachineFilter(std::function<bool(const info::machine &machine)> &&machineFilter)
{
	m_machineFilter = std::move(machineFilter);
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
	std::optional<AuditIdentifier> result;

	// check the filter
	info::machine machine = m_infoDb.machines()[position];
	if (!m_machineFilter || m_machineFilter(machine))
	{
		const QString &machineName = machine.name();
		if (m_prefs.getMachineAuditStatus(machineName) == AuditStatus::Unknown)
			result = MachineAuditIdentifier(machineName);
	}
	return result;
}
