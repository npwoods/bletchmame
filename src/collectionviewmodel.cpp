/***************************************************************************

    collectionviewmodel.cpp

    Our implementation of QAbstractItemModel

***************************************************************************/

#include <stdexcept>

#include "collectionviewmodel.h"
#include "utility.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

CollectionViewModel::CollectionViewModel(QObject *parent, Preferences &prefs, const CollectionViewDesc &desc, std::unique_ptr<ICollectionImpl> &&coll_impl, bool support_label_edit)
    : QAbstractItemModel(parent)
    , m_desc(desc)
    , m_prefs(prefs)
    , m_coll_impl(std::move(coll_impl))
{
}


//-------------------------------------------------
//  updateListView
//-------------------------------------------------

void CollectionViewModel::updateListView()
{
    beginResetModel();
    endResetModel();
}


//-------------------------------------------------
//  index
//-------------------------------------------------

QModelIndex CollectionViewModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}


//-------------------------------------------------
//  parent
//-------------------------------------------------

QModelIndex CollectionViewModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}


//-------------------------------------------------
//  rowCount
//-------------------------------------------------

int CollectionViewModel::rowCount(const QModelIndex &parent) const
{
    return m_coll_impl->GetSize();
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int CollectionViewModel::columnCount(const QModelIndex &parent) const
{
    return util::safe_static_cast<int>(m_desc.m_columns.size());
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant CollectionViewModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (index.isValid()
        && index.row() >= 0
        && index.row() < m_coll_impl->GetSize()
        && role == Qt::DisplayRole)
    {
        result = m_coll_impl->GetItemText(index.row(), index.column());
    }
    return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant CollectionViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Orientation::Horizontal || section < 0 || section >= m_desc.m_columns.size())
        return QVariant();

    const ColumnDesc &column_desc = m_desc.m_columns[section];

    QVariant result;
    switch (role)
    {
    case Qt::DisplayRole:
        result = column_desc.m_description;
        break;

    case Qt::SizeHintRole:
        {
            const std::unordered_map<std::string, ColumnPrefs> &col_prefs = m_prefs.GetColumnPrefs(m_desc.m_name);
            int width = col_prefs.find(m_desc.m_columns[section].m_id)->second.m_width;
            result = QSize(width, 20);
        }
        break;

    default:
        result = QVariant();
        break;
    }
    return result;
}
