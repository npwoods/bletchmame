/***************************************************************************

	machinelistitemmodel.h

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#ifndef MACHINELISTITEMMODEL_H
#define MACHINELISTITEMMODEL_H

#include <QAbstractItemModel>

#include "info.h"

class IconLoader;


// ======================> MachineListItemModel

class MachineListItemModel : public QAbstractItemModel
{
public:
	enum class Column
	{
		Machine,
		Description,
		Year,
		Manufacturer,
		Count
	};

	MachineListItemModel(QObject *parent, info::database &infoDb, IconLoader &iconLoader);

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
	info::database &	m_infoDb;
	IconLoader &		m_iconLoader;
};

#endif // MACHINELISTITEMMODEL_H
