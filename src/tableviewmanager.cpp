/***************************************************************************

    tableviewmanager.cpp

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

#include <QHeaderView>
#include <QLineEdit>
#include <QTableView>
#include <QSortFilterProxyModel>

#include "tableviewmanager.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

TableViewManager::TableViewManager(QTableView &tableView, QAbstractItemModel &itemModel, QLineEdit *lineEdit, Preferences &prefs, const Description &desc, std::function<void(const QString &)> &&selectionChangedCallback)
    : QObject((QObject *) &tableView)
    , m_prefs(prefs)
    , m_desc(desc)
    , m_selectionChangedCallback(std::move(selectionChangedCallback))
    , m_columnCount(-1)
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
        const QString &text = m_prefs.GetSearchBoxText(desc.m_name);
        lineEdit->setText(text);
        m_proxyModel->setFilterFixedString(text);

        // make the search box functional
        auto callback = [this, lineEdit, &tableView, descName{ desc.m_name }]()
        {
            // change the filter
            QString text = lineEdit->text();
            m_prefs.SetSearchBoxText(descName, QString(text));
            m_proxyModel->setFilterFixedString(text);

            // ensure that whatever was selected stays visible
            applySelectedValue();
        };
        connect(lineEdit, &QLineEdit::textEdited, this, callback);
    }

    // count the number of columns
    m_columnCount = 0;
    while (m_desc.m_columns[m_columnCount].m_id)
        m_columnCount++;

    // configure the header
    tableView.horizontalHeader()->setSectionsMovable(true);

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
            m_prefs.SetListViewSelection(m_desc.m_name, util::g_empty_string, std::move(selectedValue));
		}
	});

    // handle refreshing the selection
    connect(&itemModel, &QAbstractItemModel::modelReset, this, [this]()
    {
        applySelectedValue();
    });

    // handle resizing
	connect(tableView.horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int oldSize, int newSize)
	{
        if (!m_currentlyApplyingColumnPrefs)
            persistColumnPrefs();
	});

	// handle reordering
    connect(tableView.horizontalHeader(), &QHeaderView::sectionMoved, this, [this](int logicalIndex, int oldVisualIndex, int newVisualIndex)
    {
        if (!m_currentlyApplyingColumnPrefs)
            persistColumnPrefs();
    });

    // handle when sort order changes
    connect(tableView.horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order)
    {
        if (!m_currentlyApplyingColumnPrefs)
            persistColumnPrefs();
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
//  applyColumnPrefs
//-------------------------------------------------

void TableViewManager::applyColumnPrefs()
{
    // we are applying column prefs - we don't want to trample on ourselves
    m_currentlyApplyingColumnPrefs = true;

    // identify the QTableView
    QTableView &tableView = *dynamic_cast<QTableView *>(parent());

    // get the preferences
    const std::unordered_map<std::string, ColumnPrefs> &columnPrefs = m_prefs.GetColumnPrefs(m_desc.m_name);

    // unpack them
    int sortLogicalColumn = 0;
    Qt::SortOrder sortType = Qt::SortOrder::AscendingOrder;
    std::vector<int> logicalColumnOrdering;
    logicalColumnOrdering.resize(m_columnCount);
    for (int logicalColumn = 0; logicalColumn < m_columnCount; logicalColumn++)
    {
        // get the info out of preferences
        auto iter = columnPrefs.find(m_desc.m_columns[logicalColumn].m_id);
        int width = iter != columnPrefs.end() ? iter->second.m_width : m_desc.m_columns[logicalColumn].m_defaultWidth;
        int order = iter != columnPrefs.end() ? iter->second.m_order : logicalColumn;

        // track the sort
        if (iter != columnPrefs.end() && iter->second.m_sort.has_value())
        {
            sortLogicalColumn = logicalColumn;
            sortType = iter->second.m_sort.value();
        }

        // resize the column
        tableView.horizontalHeader()->resizeSection(logicalColumn, width);

        // track the order
        logicalColumnOrdering[logicalColumn] = order;
    }

    // specify the sort indicator
    tableView.horizontalHeader()->setSortIndicator(sortLogicalColumn, sortType);

    // reorder columns appropriately
    for (int column = 0; column < m_columnCount - 1; column++)
    {
        if (logicalColumnOrdering[column] != column)
        {
            auto iter = std::find(
                logicalColumnOrdering.begin() + column + 1,
                logicalColumnOrdering.end(),
                column);
            if (iter != logicalColumnOrdering.end())
            {
                // move on the header
                tableView.horizontalHeader()->moveSection(iter - logicalColumnOrdering.begin(), column);

                // update the ordering
                logicalColumnOrdering.erase(iter);
                logicalColumnOrdering.insert(logicalColumnOrdering.begin() + column, column);
            }
        }
    }

    // we're done
    m_currentlyApplyingColumnPrefs = false;
}


//-------------------------------------------------
//  persistColumnPrefs
//-------------------------------------------------

void TableViewManager::persistColumnPrefs()
{
	const QTableView &tableView = parentAsTableView();
    const QHeaderView &headerView = *tableView.horizontalHeader();

	// start preparing column prefs
	std::unordered_map<std::string, ColumnPrefs> col_prefs;
	col_prefs.reserve(m_columnCount);

	// get all info about each column
	for (int logicalColumn = 0; logicalColumn < m_columnCount; logicalColumn++)
	{
		ColumnPrefs &this_col_pref = col_prefs[m_desc.m_columns[logicalColumn].m_id];
		this_col_pref.m_width = headerView.sectionSize(logicalColumn);
		this_col_pref.m_order = headerView.visualIndex(logicalColumn);
		this_col_pref.m_sort = headerView.sortIndicatorSection() == logicalColumn
            ? headerView.sortIndicatorOrder()
            : std::optional<Qt::SortOrder>();
	}

	// and save it
	m_prefs.SetColumnPrefs(m_desc.m_name, std::move(col_prefs));
}


//-------------------------------------------------
//  applySelectedValue
//-------------------------------------------------

void TableViewManager::applySelectedValue()
{
    QTableView &tableView = *dynamic_cast<QTableView *>(parent());
    QAbstractItemModel &itemModel = *m_proxyModel->sourceModel();

    const QString &selectedValue = m_prefs.GetListViewSelection(m_desc.m_name, util::g_empty_string);
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
