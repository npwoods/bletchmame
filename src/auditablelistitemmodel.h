/***************************************************************************

	auditableistitemmodel.h

	QAbstractItemModel base class for auditables

***************************************************************************/

#ifndef AUDITABLELISTITEMMODEL_H
#define AUDITABLELISTITEMMODEL_H

// bletchmame headers
#include "audittask.h"

// Qt headers
#include <QAbstractItemModel>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> AuditableListItemModel

class AuditableListItemModel : public QAbstractItemModel
{
public:
	using QAbstractItemModel::QAbstractItemModel;

	// virtuals
	virtual Identifier getAuditIdentifier(int row) const = 0;
	virtual bool isAuditIdentifierPresent(const Identifier &identifier) const = 0;
};

#endif // AUDITABLELISTITEMMODEL_H
