/***************************************************************************

	machinelistitemmodel.cpp

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#include "machinelistitemmodel.h"
#include "iconloader.h"
#include "utility.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MachineListItemModel::MachineListItemModel(QObject *parent, info::database &infoDb, IconLoader *iconLoader, std::function<void(info::machine)> &&machineIconAccessedCallback)
	: QAbstractItemModel(parent)
	, m_infoDb(infoDb)
	, m_iconLoader(iconLoader)
	, m_machineIconAccessedCallback(machineIconAccessedCallback)
{
	m_infoDb.addOnChangedHandler([this]
	{
		populateIndexes();
	});
}


//-------------------------------------------------
//  machineFromIndex
//-------------------------------------------------

info::machine MachineListItemModel::machineFromIndex(const QModelIndex &index) const
{
	return m_infoDb.machines()[m_indexes[index.row()]];
}


//-------------------------------------------------
//  setMachineFilter
//-------------------------------------------------

void MachineListItemModel::setMachineFilter(std::function<bool(const info::machine &machine)> &&machineFilter)
{
	m_machineFilter = std::move(machineFilter);
	populateIndexes();
}


//-------------------------------------------------
//  auditStatusesChanged
//-------------------------------------------------

void MachineListItemModel::auditStatusesChanged()
{
	QModelIndex topLeft = createIndex(0, (int)Column::Machine);
	QModelIndex bottomRight = createIndex(util::safe_static_cast<int>(m_indexes.size()) - 1, (int)Column::Machine);
	QVector<int> roles = { Qt::DecorationRole };
	dataChanged(topLeft, bottomRight, roles);
}


//-------------------------------------------------
//  populateIndexes
//-------------------------------------------------

void MachineListItemModel::populateIndexes()
{
	beginResetModel();

	m_indexes.clear();
	m_indexes.reserve(m_infoDb.machines().size());
	for (int i = 0; i < m_infoDb.machines().size(); i++)
	{
		info::machine machine = m_infoDb.machines()[i];

		// we only use runnable machines
		if (machine.runnable())
		{
			// and we need to apply a filter (if we have one)
			if (!m_machineFilter || m_machineFilter(machine))
			{
				m_indexes.push_back(i);
			}
		}
	}
	m_indexes.shrink_to_fit();

	endResetModel();
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex MachineListItemModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex MachineListItemModel::parent(const QModelIndex &child) const
{
	return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int MachineListItemModel::rowCount(const QModelIndex &parent) const
{
	return util::safe_static_cast<int>(m_indexes.size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int MachineListItemModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant MachineListItemModel::data(const QModelIndex &index, int role) const
{
	QVariant result;
	if (index.isValid()
		&& index.row() >= 0
		&& index.row() < m_indexes.size())
	{
		info::machine machine = machineFromIndex(index);
		Column column = (Column)index.column();

		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Machine:
				result = machine.name();
				break;
			case Column::Description:
				result = machine.description();
				break;
			case Column::Year:
				result = machine.year();
				break;
			case Column::Manufacturer:
				result = machine.manufacturer();
				break;
			}
			break;

		case Qt::DecorationRole:
			if (column == Column::Machine)
			{
				// load this icon (assuming we have an icon loader)
				if (m_iconLoader)
					result = m_iconLoader->getIcon(machine);

				// invoke the "accessed" callback, which can trigger an audit when autoauditing
				if (m_machineIconAccessedCallback)
					m_machineIconAccessedCallback(machine);
			}
			break;
		}
	}
	return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant MachineListItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant result;
	if (orientation == Qt::Orientation::Horizontal && section >= 0 && section < util::enum_count<Column>())
	{
		Column column = (Column)section;
		switch (role)
		{
		case Qt::DisplayRole:
			switch (column)
			{
			case Column::Machine:
				result = "Machine";
				break;
			case Column::Description:
				result = "Description";
				break;
			case Column::Year:
				result = "Year";
				break;
			case Column::Manufacturer:
				result = "Manufacturer";
				break;
			}
			break;
		}
	}
	return result;
}
