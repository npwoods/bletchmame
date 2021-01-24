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

MachineListItemModel::MachineListItemModel(QObject *parent, info::database &infoDb, IIconLoader &iconLoader)
	: QAbstractItemModel(parent)
	, m_infoDb(infoDb)
    , m_iconLoader(iconLoader)
{
    m_infoDb.addOnChangedHandler([this]
    {
        beginResetModel();
        populateIndexes();
        endResetModel();
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
//  populateIndexes - we only use runnable machines
//-------------------------------------------------

void MachineListItemModel::populateIndexes()
{
    m_indexes.clear();
    m_indexes.reserve(m_infoDb.machines().size());
    for (int i = 0; i < m_infoDb.machines().size(); i++)
    {
        if (m_infoDb.machines()[i].runnable())
            m_indexes.push_back(i);
    }
    m_indexes.shrink_to_fit();
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
    return (int)Column::Count;
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
                result = m_iconLoader.getIcon(machine);
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
    if (orientation == Qt::Orientation::Horizontal && section >= 0 && section < (int)Column::Count)
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
