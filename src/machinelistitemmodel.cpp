/***************************************************************************

	machinelistitemmodel.cpp

	QAbstractItemModel implementation for the machine list

***************************************************************************/

#include "machinelistitemmodel.h"
#include "utility.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

MachineListItemModel::MachineListItemModel(QObject *parent, info::database &infoDb)
	: QAbstractItemModel(parent)
	, m_infoDb(infoDb)
{
    m_infoDb.set_on_changed([this]
    {
        beginResetModel();
        endResetModel();
    });
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
    return util::safe_static_cast<int>(m_infoDb.machines().size());
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
        && index.row() < m_infoDb.machines().size()
        && role == Qt::DisplayRole)
    {
        info::machine machine = m_infoDb.machines()[index.row()];
        Column column = (Column)index.column();
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
