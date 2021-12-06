/***************************************************************************

	auditableistitemmodel.h

	QAbstractItemModel base class for auditables

***************************************************************************/

#ifndef AUDITABLELISTITEMMODEL_H
#define AUDITABLELISTITEMMODEL_H

#include <QAbstractItemModel>
#include "audittask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> AuditableListItemModel

class AuditableListItemModel : public QAbstractItemModel
{
public:
	using QAbstractItemModel::QAbstractItemModel;

	// virtuals
	virtual AuditIdentifier getAuditIdentifier(int row) const = 0;
	virtual bool isAuditIdentifierPresent(const AuditIdentifier &identifier) const = 0;
};

#endif // AUDITABLELISTITEMMODEL_H
