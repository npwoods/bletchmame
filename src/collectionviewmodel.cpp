/***************************************************************************

    collectionviewmodel.cpp

    Our implementation of QAbstractItemModel

***************************************************************************/

#include <stdexcept>

#include <QItemSelectionModel>

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
	, m_key_column_index(-1)
	, m_sort_column(0)
    , m_sort_type(ColumnPrefs::sort_type::ASCENDING)
{
	for (int i = 0; i < desc.m_columns.size(); i++)
	{
		if (!strcmp(desc.m_columns[i].m_id, desc.m_key_column))
			m_key_column_index = i;
	}
}


//-------------------------------------------------
//  updateListView
//-------------------------------------------------

void CollectionViewModel::updateListView()
{
	// signal the model resetting
	beginResetModel();

	// get basic info about things
	long collection_size = m_coll_impl->GetSize();
	int column_count = columnCount();

	// get the filter text
	const QString &filter_text = m_prefs.GetSearchBoxText(m_desc.m_name);

	// rebuild the indirection list
	m_indirections.clear();
	m_indirections.reserve(collection_size);
	for (long i = 0; i < collection_size; i++)
	{
		// check for a match
		bool match = filter_text.isEmpty();
		for (int column = 0; !match && (column < column_count); column++)
		{
			QString cell_text = getActualItemText(i, column);
			match = util::string_icontains(cell_text.toStdWString(), filter_text.toStdWString());
		}

		if (match)
			m_indirections.push_back(i);
	}

	// sort the indirection list
	std::sort(m_indirections.begin(), m_indirections.end(), [this, column_count](int a, int b)
	{
		// first compare the actual sort
		int rc = compareActualRows(a, b, m_sort_column, m_sort_type);
		if (rc != 0)
			return rc < 0;

		// in the unlikely event of a tie, try comparing the other columns
		for (int column_index = 0; rc == 0 && column_index < column_count; column_index++)
		{
			if (column_index != m_sort_column)
				rc = compareActualRows(a, b, column_index, ColumnPrefs::sort_type::ASCENDING);
		}
		return rc < 0;
	});


	// restore the selection
	const QString &selected_item = getListViewSelection();
	const int *selected_actual_index = nullptr;
	if (!selected_item.isEmpty())
	{
		selected_actual_index = util::find_if_ptr(m_indirections, [this, &selected_item](int actual_index)
		{
			return selected_item == getActualItemText(actual_index, m_key_column_index);
		});
	}
	(void)selected_actual_index;	// TODO - actually restore selection

	// we're done!
	endResetModel();
}


//-------------------------------------------------
//  CompareActualRows
//-------------------------------------------------

int CollectionViewModel::compareActualRows(int row_a, int row_b, int sort_column, ColumnPrefs::sort_type sort_type) const
{
	const QString &a_string = getActualItemText(row_a, sort_column);
	const QString &b_string = getActualItemText(row_b, sort_column);
	int compare_result = util::string_icompare(a_string.toStdWString(), b_string.toStdWString());
	return sort_type == ColumnPrefs::sort_type::ASCENDING
		? compare_result
		: -compare_result;
}


//-------------------------------------------------
//  getActualItemText
//-------------------------------------------------

const QString &CollectionViewModel::getActualItemText(long item, long column) const
{
	return m_coll_impl->GetItemText(item, column);
}


//-------------------------------------------------
//  getListViewSelection
//-------------------------------------------------

const QString &CollectionViewModel::getListViewSelection() const
{
	return m_prefs.GetListViewSelection(m_desc.m_name, util::g_empty_string);
}


//-------------------------------------------------
//  setListViewSelection
//-------------------------------------------------

void CollectionViewModel::setListViewSelection(const QString &selection)
{
	m_prefs.SetListViewSelection(m_desc.m_name, util::g_empty_string, QString(selection));
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

int CollectionViewModel::rowCount() const
{
	return util::safe_static_cast<int>(m_indirections.size());
}


int CollectionViewModel::rowCount(const QModelIndex &parent) const
{
	return rowCount();
}


//-------------------------------------------------
//  columnCount
//-------------------------------------------------

int CollectionViewModel::columnCount() const
{
	return util::safe_static_cast<int>(m_desc.m_columns.size());
}


int CollectionViewModel::columnCount(const QModelIndex &parent) const
{
	return columnCount();
}


//-------------------------------------------------
//  data
//-------------------------------------------------

QVariant CollectionViewModel::data(const QModelIndex &index, int role) const
{
    QVariant result;
    if (index.isValid()
        && index.row() >= 0
        && index.row() < rowCount()
        && role == Qt::DisplayRole)
    {
		long actual_item = m_indirections[index.row()];
		result = getActualItemText(actual_item, index.column());
    }
    return result;
}


//-------------------------------------------------
//  headerData
//-------------------------------------------------

QVariant CollectionViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Orientation::Horizontal || section < 0 || section >= columnCount())
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


//-------------------------------------------------
//  sort
//-------------------------------------------------

void CollectionViewModel::sort(int column, Qt::SortOrder order)
{
	m_sort_column = column;
	switch (order)
	{
	case Qt::SortOrder::AscendingOrder:
		m_sort_type = ColumnPrefs::sort_type::ASCENDING;
		break;
	case Qt::SortOrder::DescendingOrder:
		m_sort_type = ColumnPrefs::sort_type::DESCENDING;
		break;
	default:
		throw false;
	}
	updateListView();
}
