/***************************************************************************

	machinelistitemmodel.h

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#ifndef MACHINELISTITEMMODEL_H
#define MACHINELISTITEMMODEL_H

#include <QAbstractItemModel>

#include "info.h"

class IIconLoader;


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

		Max = Manufacturer
	};

	MachineListItemModel(QObject *parent, info::database &infoDb, IIconLoader &iconLoader, std::function<void(info::machine)> &&machineIconAccessedCallback);

	// methods
	info::machine machineFromIndex(const QModelIndex &index) const;
	void setMachineFilter(std::function<bool(const info::machine &machine)> &&machineFilter);
	void auditStatusesChanged();

	// virtuals
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
	virtual QModelIndex parent(const QModelIndex &child) const override;
	virtual int rowCount(const QModelIndex &parent) const override;
	virtual int columnCount(const QModelIndex &parent) const override;
	virtual QVariant data(const QModelIndex &index, int role) const override;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
	info::database &									m_infoDb;
	IIconLoader &										m_iconLoader;
	std::function<bool(const info::machine &machine)>	m_machineFilter;
	std::vector<int>									m_indexes;
	std::function<void(info::machine)>					m_machineIconAccessedCallback;

	void populateIndexes();
};

#endif // MACHINELISTITEMMODEL_H
