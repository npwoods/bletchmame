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
	proxyModel.setSortLocaleAware(true);
	proxyModel.setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
	proxyModel.setFilterKeyColumn(-1);

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
			QModelIndex actualSelectedIndex = mapToSource(selectedIndexes[0]);
			const QString &val = getActualItemText(actualSelectedIndex.row(), m_key_column_index);
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
	sortFilterProxyModel().setFilterFixedString(filter_text);

	// we're done resetting the model
	endResetModel();

	// after resetting the model, restore the selection (this is probably not the Qt way to do things,
	// but so be it, at least for now)
	const QString &selected_item = getListViewSelection();
	selectItemByText(selected_item);
}


//-------------------------------------------------
//  parentAsAbstractItemView
//-------------------------------------------------

QAbstractItemView &CollectionViewModel::parentAsAbstractItemView()
{
	return *dynamic_cast<QAbstractItemView *>(QObject::parent());
}


const QAbstractItemView &CollectionViewModel::parentAsAbstractItemView() const
{
	return *dynamic_cast<const QAbstractItemView *>(QObject::parent());
}


//-------------------------------------------------
//  sortFilterProxyModel
//-------------------------------------------------

QSortFilterProxyModel &CollectionViewModel::sortFilterProxyModel()
{
	return *(dynamic_cast<QSortFilterProxyModel *>(parentAsAbstractItemView().model()));
}


const QSortFilterProxyModel &CollectionViewModel::sortFilterProxyModel() const
{
	return *(dynamic_cast<const QSortFilterProxyModel *>(parentAsAbstractItemView().model()));
}


//-------------------------------------------------
//  selectItemByText
//-------------------------------------------------

void CollectionViewModel::selectItemByText(const QString &text)
{
	std::optional<QModelIndex> selectedIndex;

	// simple heuristic - only select non-empty text
	if (!text.isEmpty())
	{
		// find the selected item
		QSortFilterProxyModel &proxyModel = sortFilterProxyModel();
		int rowCount = proxyModel.rowCount();
		for (int row = 0; !selectedIndex.has_value() && row < rowCount; row++)
		{
			QModelIndex index = proxyModel.index(row, m_key_column_index);
			QString data = proxyModel.data(index).toString();
			if (text == data)
				selectedIndex = index;
		}
	}

	// if we found a QModelIndex, select it
	if (selectedIndex.has_value())
		parentAsAbstractItemView().selectionModel()->select(selectedIndex.value(), QItemSelectionModel::Select | QItemSelectionModel::Rows);
	else
		parentAsAbstractItemView().clearSelection();
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
//  getFirstSelected - modelled after
//	wxListView::GetFirstSelected() from wxWidgets
//-------------------------------------------------

long CollectionViewModel::getFirstSelected() const
{
	// find the selection
	QModelIndex selectedIndex = parentAsAbstractItemView().currentIndex();
	QModelIndex actualSelectedIndex = mapToSource(selectedIndex);
	return actualSelectedIndex.isValid() ? actualSelectedIndex.row() : -1;
}


//-------------------------------------------------
//  mapToSource
//-------------------------------------------------

QModelIndex CollectionViewModel::mapToSource(const QModelIndex &index) const
{
	return sortFilterProxyModel().mapToSource(index);
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
	return util::safe_static_cast<int>(m_coll_impl->GetSize());
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
		result = getActualItemText(index.row(), index.column());
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
