/***************************************************************************

    tableviewmanager.cpp

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

// bletchmame headers
#include "tableviewmanager.h"
#include "dialogs/customizefields.h"
#include "prefs.h"

// Qt headers
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTableView>


//-------------------------------------------------
//  ctor
//-------------------------------------------------

TableViewManager::TableViewManager(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc, std::function<void(const QString &)> &&selectionChangedCallback)
	: QObject((QObject *) &tableView)
	, m_prefs(prefs)
	, m_desc(desc)
	, m_selectionChangedCallback(std::move(selectionChangedCallback))
	, m_proxyModel(nullptr)
	, m_currentlyApplyingColumnPrefs(false)
{
	// create a proxy model for sorting
	m_proxyModel = new QSortFilterProxyModel((QObject *)&tableView);
	m_proxyModel->setSourceModel(&itemModel);
	m_proxyModel->setSortCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
	m_proxyModel->setSortLocaleAware(true);
	m_proxyModel->setFilterCaseSensitivity(Qt::CaseSensitivity::CaseInsensitive);
	m_proxyModel->setFilterKeyColumn(-1);

	// do we have a search box?
	if (lineEdit)
	{
		// set the initial text on the search box
		const QString &text = m_prefs.getSearchBoxText(desc.m_name);
		lineEdit->setText(text);
		m_proxyModel->setFilterFixedString(text);

		// make the search box functional
		auto callback = [this, lineEdit, descName{ desc.m_name }]()
		{
			// change the filter
			QString text = lineEdit->text();
			m_prefs.setSearchBoxText(descName, QString(text));
			m_proxyModel->setFilterFixedString(text);

			// ensure that whatever was selected stays visible
			applySelectedValue();
		};
		connect(lineEdit, &QLineEdit::textEdited, this, callback);
	}

	// configure the header
	QHeaderView &horizontalHeader = *tableView.horizontalHeader();
	horizontalHeader.setSectionsMovable(true);
	horizontalHeader.setContextMenuPolicy(Qt::CustomContextMenu);

	// set the model
	tableView.setModel(m_proxyModel);

	// handle selection
	connect(tableView.selectionModel(), &QItemSelectionModel::selectionChanged, this, [this, &itemModel](const QItemSelection &newSelection, const QItemSelection &oldSelection)
	{
		QModelIndexList selectedIndexes = newSelection.indexes();
		if (!selectedIndexes.empty())
		{
			int selectedRow = m_proxyModel->mapToSource(selectedIndexes[0]).row();
			QModelIndex selectedIndex = itemModel.index(selectedRow, m_desc.m_keyColumnIndex);
			QString selectedValue = itemModel.data(selectedIndex).toString();
			if (m_selectionChangedCallback)
				m_selectionChangedCallback(selectedValue);
			m_prefs.setListViewSelection(m_desc.m_name, util::g_empty_string, std::move(selectedValue));
		}
	});

	// handle refreshing the selection
	connect(&itemModel, &QAbstractItemModel::modelReset, this, [this]()
	{
		applySelectedValue();
	});

	// handle resizing
	connect(&horizontalHeader, &QHeaderView::sectionResized, this, [this](int logicalIndex, int oldSize, int newSize)
	{
		if (!m_currentlyApplyingColumnPrefs)
			persistColumnPrefs();
	});

	// handle reordering
	connect(&horizontalHeader, &QHeaderView::sectionMoved, this, [this](int logicalIndex, int oldVisualIndex, int newVisualIndex)
	{
		if (!m_currentlyApplyingColumnPrefs)
			persistColumnPrefs();
	});

	// handle when sort order changes
	connect(&horizontalHeader, &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order)
	{
		if (!m_currentlyApplyingColumnPrefs)
			persistColumnPrefs();
	});

	// handle context menu
	connect(&horizontalHeader, &QHeaderView::customContextMenuRequested, this, [this](const QPoint &pos)
	{
		headerContextMenuRequested(pos);
	});
}


//-------------------------------------------------
//  setup
//-------------------------------------------------

TableViewManager &TableViewManager::setup(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc, std::function<void(const QString &)> &&selectionChangedCallback)
{
	// set up a TableViewManager
	TableViewManager &manager = *new TableViewManager(tableView, itemModel, lineEdit, prefs, desc, std::move(selectionChangedCallback));

	// and read the prefs
	manager.applyColumnPrefs();

	// and return
	return manager;
}


//-------------------------------------------------
//  parentAsTableView
//-------------------------------------------------

const QTableView &TableViewManager::parentAsTableView() const
{
	return *dynamic_cast<const QTableView *>(QObject::parent());
}


//-------------------------------------------------
//  parentAsTableView
//-------------------------------------------------

QTableView &TableViewManager::parentAsTableView()
{
	return *dynamic_cast<QTableView *>(QObject::parent());
}


//-------------------------------------------------
//  applyColumnPrefs
//-------------------------------------------------

void TableViewManager::applyColumnPrefs()
{
	// we are applying column prefs - we don't want to trample on ourselves
	m_currentlyApplyingColumnPrefs = true;

	// identify the header
	QHeaderView &horizontalHeader = *parentAsTableView().horizontalHeader();

	// in absence of anything in the preferences, this is the default sort column and direction
	int sortLogicalColumn = 0;
	Qt::SortOrder sortType = Qt::SortOrder::AscendingOrder;

	// and in absence of anything in the preferences, set up the default sort order
	std::vector<std::optional<int>> logicalColumnOrdering;
	logicalColumnOrdering.reserve(m_desc.m_columns.size());

	// get the preferences
	const std::unordered_map<std::u8string, ColumnPrefs> &columnPrefs = m_prefs.getColumnPrefs(m_desc.m_name);

	// unpack them
	for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
	{
		// defaults
		int width = m_desc.m_columns[logicalColumn].m_defaultWidth;
		std::optional<int> order = m_desc.m_columns[logicalColumn].m_defaultShown
			? logicalColumn
			: std::optional<int>();

		// get the info out of preferences
		auto iter = columnPrefs.find(m_desc.m_columns[logicalColumn].m_id);
		if (iter != columnPrefs.end())
		{
			width = iter->second.m_width;
			order = iter->second.m_order;

			if (iter->second.m_sort.has_value())
			{
				sortLogicalColumn = logicalColumn;
				sortType = iter->second.m_sort.value();
			}
		}

		// resize the column
		horizontalHeader.resizeSection(logicalColumn, width);

		// set the ordering
		logicalColumnOrdering.push_back(order);
	}

	// specify the sort indicator
	horizontalHeader.setSortIndicator(sortLogicalColumn, sortType);
	m_proxyModel->sort(sortLogicalColumn, sortType);

	// reorder columns appropriately
	applyColumnOrdering(logicalColumnOrdering);

	// we're done
	m_currentlyApplyingColumnPrefs = false;
}


//-------------------------------------------------
//  applyColumnOrdering
//-------------------------------------------------

void TableViewManager::applyColumnOrdering(std::span<const std::optional<int>> logicalOrdering)
{
	// these really need to be the same size
	assert(logicalOrdering.size() == m_desc.m_columns.size());

	// identify the header
	QHeaderView &horizontalHeader = *parentAsTableView().horizontalHeader();

	// first set the visibility
	for (int logicalIndex = 0; logicalIndex < logicalOrdering.size(); logicalIndex++)
		horizontalHeader.setSectionHidden(logicalIndex, !logicalOrdering[logicalIndex].has_value());

	// now move the sections around
	for (int logicalIndex = 0; logicalIndex < logicalOrdering.size(); logicalIndex++)
	{
		if (logicalOrdering[logicalIndex].has_value())
		{
			int sectionFrom = horizontalHeader.visualIndex(logicalIndex);
			int sectionTo = logicalOrdering[logicalIndex].value();
			if (sectionFrom != sectionTo)
				horizontalHeader.moveSection(sectionFrom, sectionTo);
		}
	}
}


//-------------------------------------------------
//  persistColumnPrefs
//-------------------------------------------------

void TableViewManager::persistColumnPrefs()
{
	const QTableView &tableView = parentAsTableView();
	const QHeaderView &headerView = *tableView.horizontalHeader();

	// start preparing column prefs
	std::unordered_map<std::u8string, ColumnPrefs> col_prefs;
	col_prefs.reserve(m_desc.m_columns.size());

	// get all info about each column
	for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
	{
		ColumnPrefs &this_col_pref = col_prefs[m_desc.m_columns[logicalColumn].m_id];
		this_col_pref.m_width = headerView.sectionSize(logicalColumn);
		this_col_pref.m_order = headerView.isSectionHidden(logicalColumn)
			? std::optional<int>()
			: headerView.visualIndex(logicalColumn);
		this_col_pref.m_sort = headerView.sortIndicatorSection() == logicalColumn
			? headerView.sortIndicatorOrder()
			: std::optional<Qt::SortOrder>();
	}

	// and save it
	m_prefs.setColumnPrefs(m_desc.m_name, std::move(col_prefs));
}


//-------------------------------------------------
//  applySelectedValue
//-------------------------------------------------

void TableViewManager::applySelectedValue()
{
	QTableView &tableView = parentAsTableView();
	QAbstractItemModel &itemModel = *m_proxyModel->sourceModel();

	const QString &selectedValue = m_prefs.getListViewSelection(m_desc.m_name, util::g_empty_string);
	QModelIndex selectedIndex;

	if (!selectedValue.isEmpty())
	{
		QModelIndexList matches = itemModel.match(itemModel.index(0, 0), Qt::DisplayRole, selectedValue, 9999);
		for (const QModelIndex &match : matches)
		{
			if (match.column() == m_desc.m_keyColumnIndex)
			{
				selectedIndex = m_proxyModel->mapFromSource(match);
				break;
			}
		}
	}

	// we've figured out what we want to select (if anything), do it!
	if (selectedIndex.isValid())
	{
		// not sure why we have to call scrollTo() twice, but it doesn't
		// seem to always register without it
		tableView.scrollTo(selectedIndex);
		tableView.selectRow(selectedIndex.row());
		tableView.scrollTo(selectedIndex);
	}
	else
	{
		tableView.clearSelection();
	}
}


//-------------------------------------------------
//  headerContextMenuRequested
//-------------------------------------------------

void TableViewManager::headerContextMenuRequested(const QPoint &pos)
{
	QHeaderView &horizontalHeader = *parentAsTableView().horizontalHeader();
	int currentSortSection = horizontalHeader.sortIndicatorSection();
	int contextMenuSection = horizontalHeader.logicalIndexAt(pos);

	// wrap logic for adding sort order items
	auto addSortMenuItem = [&horizontalHeader, currentSortSection, contextMenuSection](QMenu &popupMenu, const QString &text, Qt::SortOrder sortOrder)
	{
		QAction &action = *popupMenu.addAction(text, [&horizontalHeader, contextMenuSection, sortOrder]()
		{
			horizontalHeader.setSortIndicator(contextMenuSection, sortOrder);
		});
		action.setCheckable(true);
		action.setChecked(currentSortSection == contextMenuSection && horizontalHeader.sortIndicatorOrder() == sortOrder);
	};

	// build the popup menu
	QMenu popupMenu;
	addSortMenuItem(popupMenu, "Sort Ascending", Qt::SortOrder::AscendingOrder);
	addSortMenuItem(popupMenu, "Sort Descending", Qt::SortOrder::DescendingOrder);
	popupMenu.addSeparator();
	popupMenu.addAction("Customize Fields...", [this]() { customizeFields(); });

	// and display it
	QPoint popupPos = parentAsTableView().mapToGlobal(pos);
	popupMenu.exec(popupPos);
}


//-------------------------------------------------
//  customizeFields
//-------------------------------------------------

void TableViewManager::customizeFields()
{
	// create the dialog and set other stuff up
	QTableView &tableView = parentAsTableView();
	CustomizeFieldsDialog dialog(&tableView);
	QAbstractItemModel &tableModel = *tableView.model();
	const QHeaderView &headerView = *tableView.horizontalHeader();

	// add each of the fields
	for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
	{
		// get the name and order of this column
		QString name = tableModel.headerData(logicalColumn, Qt::Orientation::Horizontal).toString();
		std::optional<int> order = headerView.isSectionHidden(logicalColumn)
			? std::optional<int>()
			: headerView.visualIndex(logicalColumn);

		// and add it
		dialog.addField(std::move(name), order);
	}
	dialog.updateStringViews();

	// run the dialog
	if (dialog.exec() == QDialog::DialogCode::Accepted)
	{
		std::vector<std::optional<int>> logicalOrdering;
		logicalOrdering.reserve(dialog.fields().size());

		for (const auto &field : dialog.fields())
			logicalOrdering.push_back(field.m_order);

		applyColumnOrdering(logicalOrdering);
	}
}
