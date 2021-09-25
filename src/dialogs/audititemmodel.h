/***************************************************************************

	dialogs/audititemmodel.h

	Auditing Dialog Item Model

***************************************************************************/

#ifndef DIALOGS_AUDITITEMMODEL_H
#define DIALOGS_AUDITITEMMODEL_H

#include <QAbstractItemModel>

#include "../audit.h"


// ======================> AuditItemModel

class AuditItemModel : public QAbstractItemModel
{
public:
	enum class Column
	{
		Media,
		Size,
		Necessity,
		Status,

		Max = Status
	};

	// ctor
	AuditItemModel(const Audit &audit, QObject *parent = nullptr);

	// methods
	void singleMediaAudited(int entryIndex, const Audit::Verdict &verdict);

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override final;
	virtual QModelIndex parent(const QModelIndex &child) const override final;
	virtual int rowCount(const QModelIndex &parent) const override final;
	virtual int columnCount(const QModelIndex &parent) const override final;
	virtual QVariant data(const QModelIndex &index, int role) const override final;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override final;

private:
	const Audit &								m_audit;
	std::vector<std::optional<Audit::Verdict>>	m_verdicts;
};


#endif // DIALOGS_AUDITITEMMODEL_H
