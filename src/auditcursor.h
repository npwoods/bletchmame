/***************************************************************************

	auditcursor.h

	Maintains a cursor through auditables

***************************************************************************/

#ifndef AUDITCURSOR_H
#define AUDITCURSOR_H

// bletchmame headers
#include "auditablelistitemmodel.h"
#include "info.h"

// standard headers
#include <optional>

class Preferences;

// ======================> AuditCursor

class AuditCursor : public QObject
{
	Q_OBJECT
public:
	// ctor
	AuditCursor(Preferences &prefs, QObject *parent = nullptr);

	// accessors
	int currentPosition() const { assert(m_position >= 0); return m_position; }
	bool isComplete() const { return m_position < 0; }

	// methods
	void setListItemModel(AuditableListItemModel *model);
	std::optional<Identifier> next(int basePosition);

private:
	Preferences &							m_prefs;
	AuditableListItemModel *				m_model;
	std::optional<QMetaObject::Connection>	m_modelResetConnection;
	int										m_position;

	// private methods
	std::optional<Identifier> getIdentifierAtCurrentPosition() const;
	void onModelReset();
};


#endif // AUDITCURSOR_H
