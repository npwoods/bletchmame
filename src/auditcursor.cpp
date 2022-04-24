/***************************************************************************

	auditcursor.cpp

	Maintains a cursor through auditables

***************************************************************************/

// bletchmame headers
#include "auditcursor.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

AuditCursor::AuditCursor(Preferences &prefs, QObject *parent)
	: QObject(parent)
	, m_prefs(prefs)
	, m_model(nullptr)
	, m_position(-1)
{
}


//-------------------------------------------------
//  next
//-------------------------------------------------

void AuditCursor::setListItemModel(AuditableListItemModel *model)
{
	if (model != m_model)
	{
		// disconnect all connections
		if (m_modelResetConnection.has_value())
		{
			disconnect(m_modelResetConnection.value());
			m_modelResetConnection.reset();
		}

		// set the model
		m_model = model;

		// did we get assigned a model?
		if (m_model)
		{
			// if so we have to do some connections
			m_position = 0;
			m_modelResetConnection = connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { onModelReset(); });
		}
		else
		{
			m_position = -1;
		}
	}
}


//-------------------------------------------------
//  next
//-------------------------------------------------

std::optional<AuditIdentifier> AuditCursor::next(int basePosition)
{
	std::optional<AuditIdentifier> result;

	// basic sanity checks
	assert(m_model);
	assert(basePosition >= 0);

	// get the row count
	int rowCount = m_model->rowCount();
	if (rowCount == 0)
		m_position = -1;

	// do nothing if we're not complete
	if (m_position >= 0)
	{
		// recalibrate m_position and basePosition
		m_position %= rowCount;
		basePosition %= rowCount;

		// try to find the next auditable
		while (m_position >= 0 && !result.has_value())
		{
			// get the value at the cursor
			result = getIdentifierAtCurrentPosition();

			// and advance the cursor
			m_position = (m_position + 1) % rowCount;

			// if we're back to the base position, mark ourselves complete
			if (m_position == basePosition)
				m_position = -1;
		}
	}

	// we're done - maybe we have one, maybe we don't
	return result;
}


//-------------------------------------------------
//  getIdentifierAtCurrentPosition
//-------------------------------------------------

std::optional<AuditIdentifier> AuditCursor::getIdentifierAtCurrentPosition() const
{
	// sanity checks
	assert(m_position >= 0);
	assert(m_model);

	// get an identifier
	AuditIdentifier identifier = m_model->getAuditIdentifier(m_position);

	// get the audit status
	std::optional<AuditStatus> status;
	std::visit(util::overloaded
	{
		[this, &status](const MachineAuditIdentifier &identifier)
		{
			status = m_prefs.getMachineAuditStatus(identifier.machineName());
		},
		[this, &status](const SoftwareAuditIdentifier &identifier)
		{
			status = m_prefs.getSoftwareAuditStatus(identifier.softwareList(), identifier.software());
		}
	}, identifier);

	// only return the identifier if the audit status is unknown
	return status == AuditStatus::Unknown
		? std::move(identifier)
		: std::optional<AuditIdentifier>();
}


//-------------------------------------------------
//  onModelReset
//-------------------------------------------------

void AuditCursor::onModelReset()
{
	// someone reset the model; awaken if need be
	if (m_position < 0)
		m_position = 0;
}
