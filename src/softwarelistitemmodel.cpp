/***************************************************************************

	softwarelistitemmodel.cpp

	QAbstractItemModel implementation for the software list

***************************************************************************/

#include "softwarelistitemmodel.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

SoftwareListItemModel::SoftwareListItemModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}


//-------------------------------------------------
//  load
//-------------------------------------------------

void SoftwareListItemModel::load(const software_list_collection &software_col, bool load_parts, const QString &dev_interface)
{
    beginResetModel();

    // clear things out
    internalReset();

    // now enumerate through each list and build the m_parts vector
    for (const software_list &softlist : software_col.software_lists())
    {
        // if the name of this software list is not in m_softlist_names, add it
        if (std::find(m_softlist_names.begin(), m_softlist_names.end(), softlist.name()) == m_softlist_names.end())
            m_softlist_names.push_back(softlist.name());

        // now enumerate through all software
        for (const software_list::software &software : softlist.get_software())
        {
            if (load_parts)
            {
                // we're loading individual parts; enumerate through them and add them
                for (const software_list::part &part : software.m_parts)
                {
                    if (dev_interface.isEmpty() || dev_interface == part.m_interface)
                        m_parts.emplace_back(software, &part);
                }
            }
            else
            {
                // we're not loading individual parts
                assert(dev_interface.isEmpty());
                m_parts.emplace_back(software, nullptr);
            }
        }
    }

    endResetModel();
}


//-------------------------------------------------
//  reset
//-------------------------------------------------

void SoftwareListItemModel::reset()
{
    beginResetModel();
    internalReset();
    endResetModel();
}


//-------------------------------------------------
//  internalReset
//-------------------------------------------------

void SoftwareListItemModel::internalReset()
{
    m_parts.clear();
    m_softlist_names.clear();
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex SoftwareListItemModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex SoftwareListItemModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int SoftwareListItemModel::rowCount(const QModelIndex &parent) const
{
    return util::safe_static_cast<int>(m_parts.size());
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int SoftwareListItemModel::columnCount(const QModelIndex &parent) const
{
	return util::enum_count<Column>();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant SoftwareListItemModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (index.isValid()
        && index.row() >= 0
        && index.row() < m_parts.size()
        && role == Qt::DisplayRole)
    {
        const software_list::software &sw = m_parts[index.row()].software();

        Column column = (Column)index.column();
        auto x = sw.m_name.toStdString();
        switch (column)
        {
        case Column::Name:
            result = sw.m_name;
            break;
        case Column::Description:
            result = sw.m_description;
            break;
        case Column::Year:
            result = sw.m_year;
            break;
        case Column::Manufacturer:
            result = sw.m_publisher;
            break;
        }
    }
    return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant SoftwareListItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant result;
    if (orientation == Qt::Orientation::Horizontal)
    {
        Column column = (Column)section;
        switch (role)
        {
        case Qt::DisplayRole:
            switch (column)
            {
            case Column::Name:
                result = "Name";
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
//  SoftwareAndPart ctor
//-------------------------------------------------

SoftwareListItemModel::SoftwareAndPart::SoftwareAndPart(const software_list::software &sw, const software_list::part *p)
    : m_software(sw)
    , m_part(p)
{
}
