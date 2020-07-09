/***************************************************************************

    collectionviewmodel.cpp

    Our implementation of QAbstractItemModel

***************************************************************************/

#include <stdexcept>

#include <QItemSelectionModel>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>

#include "collectionviewmodel.h"
#include "tableviewmanager.h"
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
{
	// figure out the key column index
	for (int i = 0; i < desc.m_columns.size(); i++)
	{
		if (!strcmp(desc.m_columns[i].m_id, desc.m_key_column))
			m_key_column_index = i;
	}

	// create a proxy model for sorting
	QSortFilterProxyModel &proxyModel = *new QSortFilterProxyModel(&tableView);
	proxyModel.setSourceModel(this);
	proxyModel.setSortCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);

	// set the model on the actual tab view
	tableView.setModel(&proxyModel);

	// set up the table view manager
	(void) new TableViewManager(tableView, m_prefs, m_desc);

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

    QVariant result;
    switch (role)
    {
    case Qt::DisplayRole:
        result = m_desc.m_columns[section].m_description;
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

#if 0
void CollectionViewModel::sort(int displayColumn, Qt::SortOrder order)
{
	updateListView();
}
#endif
