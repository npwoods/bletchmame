/***************************************************************************

	dialogs/audititemmodel.h

	Auditing Dialog Item Model

***************************************************************************/

#ifndef DIALOGS_AUDITITEMMODEL_H
#define DIALOGS_AUDITITEMMODEL_H

#include <QAbstractItemModel>

#include "../audit.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class IconLoader;

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
	AuditItemModel(const Audit &audit, IconLoader &iconLoader, QObject *parent = nullptr);

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
	IconLoader &								m_iconLoader;
	std::vector<std::optional<Audit::Verdict>>	m_verdicts;

	// statics
	static std::u8string_view iconFromAuditEntryType(Audit::Entry::Type entryType);
	static AuditStatus auditStatusFromVerdict(const std::optional<Audit::Verdict> &verdict, bool optional);
};


#endif // DIALOGS_AUDITITEMMODEL_H
