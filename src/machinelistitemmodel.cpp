/***************************************************************************

	machinelistitemmodel.cpp

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#include "machinelistitemmodel.h"
#include "audittask.h"
#include "iconloader.h"
#include "perfprofiler.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

MachineListItemModel::MachineListItemModel(QObject *parent, info::database &infoDb, IconLoader *iconLoader, std::function<void(info::machine)> &&machineIconAccessedCallback)
	: AuditableListItemModel(parent)
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
//  machineFromRow
//-------------------------------------------------

info::machine MachineListItemModel::machineFromRow(int row) const
{
	return m_infoDb.machines()[m_indexes[row]];
}


//-------------------------------------------------
//  machineFromIndex
//-------------------------------------------------

info::machine MachineListItemModel::machineFromIndex(const QModelIndex &index) const
{
	return machineFromRow(index.row());
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
//  isMachinePresent
//-------------------------------------------------

bool MachineListItemModel::isMachinePresent(const info::machine &machine) const
{
	return !m_machineFilter || m_machineFilter(machine);
}


//-------------------------------------------------
//  auditStatusChanged
//-------------------------------------------------

void MachineListItemModel::auditStatusChanged(const MachineAuditIdentifier &identifier)
{
	ProfilerScope prof(CURRENT_FUNCTION);
	auto iter = m_reverseIndexes.find(identifier.machineName());
	if (iter != m_reverseIndexes.end())
		iconsChanged(iter->second, iter->second);
}


//-------------------------------------------------
//  allAuditStatusesChanged
//-------------------------------------------------

void MachineListItemModel::allAuditStatusesChanged()
{
	ProfilerScope prof(CURRENT_FUNCTION);
	if (!m_indexes.empty())
		iconsChanged(0, util::safe_static_cast<int>(m_indexes.size()) - 1);
}


//-------------------------------------------------
//  iconsChanged
//-------------------------------------------------

void MachineListItemModel::iconsChanged(int startIndex, int endIndex)
{
	// sanity checks
	assert(startIndex >= 0 && startIndex < m_indexes.size());
	assert(endIndex >= 0 && endIndex < m_indexes.size());
	assert(startIndex <= endIndex);

	// emit a dataChanged event for decorations int he proper range
	QModelIndex topLeft = createIndex(startIndex, (int)Column::Machine);
	QModelIndex bottomRight = createIndex(endIndex, (int)Column::Machine);
	QVector<int> roles = { Qt::DecorationRole };
	emit dataChanged(topLeft, bottomRight, roles);
}


//-------------------------------------------------
//  populateIndexes
//-------------------------------------------------

void MachineListItemModel::populateIndexes()
{
	ProfilerScope prof(CURRENT_FUNCTION);
	beginResetModel();

	// prep the indexes
	m_indexes.clear();
	m_indexes.reserve(m_infoDb.machines().size());
	m_reverseIndexes.clear();
	m_reverseIndexes.reserve(m_indexes.size());

	// add all indexes
	for (int i = 0; i < m_infoDb.machines().size(); i++)
	{
		info::machine machine = m_infoDb.machines()[i];

		// we need to apply a filter (if we have one)
		if (isMachinePresent(machine))
		{
			m_reverseIndexes.insert({ std::reference_wrapper<const QString>(machine.name()), util::safe_static_cast<int>(m_indexes.size()) });
			m_indexes.push_back(i);
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
	ProfilerScope prof(CURRENT_FUNCTION);

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
					result = m_iconLoader->getIcon(machine, std::nullopt);

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


//-------------------------------------------------
//  getAuditIdentifier
//-------------------------------------------------

AuditIdentifier MachineListItemModel::getAuditIdentifier(int row) const
{
	return MachineAuditIdentifier(machineFromRow(row).name());
}


//-------------------------------------------------
//  isAuditIdentifierPresent
//-------------------------------------------------

bool MachineListItemModel::isAuditIdentifierPresent(const AuditIdentifier &identifier) const
{
	// first check to see if this is a MachineAuditIdentifier
	const MachineAuditIdentifier *machineAuditIdentifier = std::get_if<MachineAuditIdentifier>(&identifier);
	if (!machineAuditIdentifier)
		return false;

	// now try to find the machine
	std::optional<info::machine> machine = m_infoDb.find_machine(machineAuditIdentifier->machineName());
	if (!machine)
		return false;

	return isMachinePresent(machine.value());
}
