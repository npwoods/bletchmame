/***************************************************************************

    collectionviewmodel.cpp

    Our implementation of QAbstractItemModel

***************************************************************************/

#include <stdexcept>

#include <QItemSelectionModel>
#include <QTableView>
#include <QHeaderView>

#include "collectionviewmodel.h"
#include "utility.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

CollectionViewModel::CollectionViewModel(QTableView &tableView, Preferences &prefs, const CollectionViewDesc &desc, std::unique_ptr<ICollectionImpl> &&coll_impl, bool support_label_edit)
    : QAbstractItemModel(&tableView)
    , m_desc(desc)
    , m_prefs(prefs)
    , m_coll_impl(std::move(coll_impl))
	, m_key_column_index(-1)
	, m_sort_column(0)
    , m_sort_type(ColumnPrefs::sort_type::ASCENDING)
{
	// figure out the key column index
	for (int i = 0; i < desc.m_columns.size(); i++)
	{
		if (!strcmp(desc.m_columns[i].m_id, desc.m_key_column))
			m_key_column_index = i;
	}

	// set the model on the actual tab view
	tableView.setModel(this);

	// set column width and order
	{
		const std::unordered_map<std::string, ColumnPrefs> &col_prefs = m_prefs.GetColumnPrefs(m_desc.m_name);
		std::vector<int> logicalColumnWidths;
		m_columnOrder.resize(m_desc.m_columns.size());
		logicalColumnWidths.resize(m_desc.m_columns.size());

		// unpack the preferences
		for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
		{
			auto iter = col_prefs.find(m_desc.m_columns[logicalColumn].m_id);
			if (iter != col_prefs.end())
			{
				logicalColumnWidths[logicalColumn] = iter->second.m_width;
				m_columnOrder[logicalColumn] = iter->second.m_order;
			}
			else
			{
				logicalColumnWidths[logicalColumn] = m_desc.m_columns[logicalColumn].m_default_width;
				m_columnOrder[logicalColumn] = logicalColumn;
			}
		}

		// do a check to see if all display columns are found; if not, we need to reset the orders
		for (int displayColumn = 0; displayColumn < m_desc.m_columns.size(); displayColumn++)
		{
			if (std::find(m_columnOrder.begin(), m_columnOrder.end(), displayColumn) == m_columnOrder.end())
			{
				// sigh, we need to reset the order
				for (int i = 0; i < m_columnOrder.size(); i++)
					m_columnOrder[i] = i;
				break;
			}
		}

		// now finally set the sizes
		for (int logicalColumn = 0; logicalColumn < m_columnOrder.size(); logicalColumn++)
			tableView.setColumnWidth(m_columnOrder[logicalColumn], logicalColumnWidths[logicalColumn]);
	}

	// handle selection
	connect(tableView.selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		QModelIndexList selectedIndexes = newSelection.indexes();
		if (!selectedIndexes.empty())
		{
			long actual_item = m_indirections[selectedIndexes[0].row()];
			const QString &val = getActualItemText(actual_item, m_key_column_index);
			setListViewSelection(val);
		}
	});

	// handle resizing
	connect(tableView.horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int oldSize, int newSize)
	{
		updateColumnPrefs();
	});
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

	// we're done resetting the model
	endResetModel();

	// after resetting the model, restore the selection (this is probably not the Qt way to do things,
	// but so be it, at least for now)
	const QString &selected_item = getListViewSelection();
	const int *selected_actual_index = nullptr;
	if (!selected_item.isEmpty())
	{
		selected_actual_index = util::find_if_ptr(m_indirections, [this, &selected_item](int actual_index)
		{
			return selected_item == getActualItemText(actual_index, m_key_column_index);
		});
	}
	selectByIndex(selected_actual_index ? selected_actual_index - &m_indirections[0] : -1);
}


//-------------------------------------------------
//  parentAsTableView
//-------------------------------------------------

QTableView &CollectionViewModel::parentAsTableView()
{
	return *dynamic_cast<QTableView *>(QObject::parent());
}


const QTableView &CollectionViewModel::parentAsTableView() const
{
	return *dynamic_cast<const QTableView *>(QObject::parent());
}


//-------------------------------------------------
//  selectByIndex
//-------------------------------------------------

void CollectionViewModel::selectByIndex(long item_index)
{
	if (item_index < 0)
	{
		parentAsTableView().clearSelection();
	}
	else
	{
		QModelIndex modelIndex = index(item_index, 0, QModelIndex());
		parentAsTableView().selectionModel()->select(modelIndex, QItemSelectionModel::Select | QItemSelectionModel::Rows);
	}
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
//  updateColumnPrefs
//-------------------------------------------------

void CollectionViewModel::updateColumnPrefs()
{
	const QTableView &tableView = parentAsTableView();

	// get the column count
	int column_count = columnCount();

	// start preparing column prefs
	std::unordered_map<std::string, ColumnPrefs> col_prefs;
	col_prefs.reserve(column_count);

	// get all info about each column
	for (int logicalColumn = 0; logicalColumn < column_count; logicalColumn++)
	{
		int displayColumn = displayFromLogicalColumn(logicalColumn);
		int width = tableView.columnWidth(displayColumn);
		ColumnPrefs &this_col_pref = col_prefs[m_desc.m_columns[logicalColumn].m_id];
		this_col_pref.m_width = tableView.columnWidth(displayColumn);
		this_col_pref.m_order = displayColumn;
		this_col_pref.m_sort = m_sort_column == logicalColumn ? m_sort_type : std::optional<ColumnPrefs::sort_type>();
	}

	// and save it
	m_prefs.SetColumnPrefs(m_desc.m_name, std::move(col_prefs));
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
//  getFirstSelected
//-------------------------------------------------

long CollectionViewModel::getFirstSelected() const
{
	// modelled after wxListView::GetFirstSelected() from wxWidgets
	return parentAsTableView().currentIndex().row();
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
		// resolve the column order
		int logicalColumn = logicalFromDisplayColumn(index.column());

		long actual_item = m_indirections[index.row()];
		result = getActualItemText(actual_item, logicalColumn);
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

	// resolve the column order
	int logicalColumn = logicalFromDisplayColumn(section);

    QVariant result;
    switch (role)
    {
    case Qt::DisplayRole:
        result = m_desc.m_columns[logicalColumn].m_description;
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

void CollectionViewModel::sort(int displayColumn, Qt::SortOrder order)
{
	int logicalColumn = logicalFromDisplayColumn(displayColumn);
	m_sort_column = logicalColumn;
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


//-------------------------------------------------
//  logicalFromDisplayColumn
//-------------------------------------------------

int CollectionViewModel::logicalFromDisplayColumn(int displayColumn) const
{
	auto iter = std::find(m_columnOrder.begin(), m_columnOrder.end(), displayColumn);
	int logicalColumn = iter - m_columnOrder.begin();
	assert(logicalColumn >= 0 && logicalColumn <= m_columnOrder.size());
	return logicalColumn;
}


//-------------------------------------------------
//  displayFromLogicalColumn
//-------------------------------------------------

int CollectionViewModel::displayFromLogicalColumn(int logicalColumn) const
{
	return m_columnOrder[logicalColumn];
}
