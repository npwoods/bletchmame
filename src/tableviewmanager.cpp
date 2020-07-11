/***************************************************************************

    tableviewmanager.cpp

    Governs behaviors of tab views, and syncs with preferences

***************************************************************************/

#include <QTableView>
#include <QHeaderView>

#include "collectionviewmodel.h"
#include "tableviewmanager.h"
#include "prefs.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

TableViewManager::TableViewManager(QTableView &tableView, Preferences &prefs, const CollectionViewDesc &desc)
    : QObject((QObject *) &tableView)
    , m_prefs(prefs)
    , m_desc(desc)
{
    // get the preferences
    const std::unordered_map<std::string, ColumnPrefs> &columnPrefs = m_prefs.GetColumnPrefs(m_desc.m_name);

    // unpack them
    int sortLogicalColumn = 0;
    Qt::SortOrder sortType = Qt::SortOrder::AscendingOrder;
    std::vector<int> logicalColumnOrdering;
    logicalColumnOrdering.resize(m_desc.m_columns.size());
    for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
    {
        // get the info out of preferences
        auto iter = columnPrefs.find(m_desc.m_columns[logicalColumn].m_id);
        int width = iter != columnPrefs.end() ? iter->second.m_width : m_desc.m_columns[logicalColumn].m_default_width;
        int order = iter != columnPrefs.end() ? iter->second.m_order : logicalColumn;

        // resize the column
        tableView.horizontalHeader()->resizeSection(logicalColumn, width);

        // track the sort
        if (iter != columnPrefs.end() && iter->second.m_sort.has_value())
        {
            sortLogicalColumn = logicalColumn;
            sortType = iter->second.m_sort.value();
        }

        // track the order
        logicalColumnOrdering[logicalColumn] = order;
    }

    // configure the header
    tableView.horizontalHeader()->setSectionsMovable(true);
    tableView.horizontalHeader()->setSortIndicator(sortLogicalColumn, sortType);

    // reorder columns appropriately
    for (int column = 0; column < m_desc.m_columns.size() - 1; column++)
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

    // handle resizing
	connect(tableView.horizontalHeader(), &QHeaderView::sectionResized, this, [this](int logicalIndex, int oldSize, int newSize)
	{
        persistColumnPrefs();
	});

	// handle reordering
    connect(tableView.horizontalHeader(), &QHeaderView::sectionMoved, this, [this](int logicalIndex, int oldVisualIndex, int newVisualIndex)
    {
        persistColumnPrefs();
    });

    // handle when sort order changes
    connect(tableView.horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int logicalIndex, Qt::SortOrder order)
    {
        persistColumnPrefs();
    });
}


//-------------------------------------------------
//  parentAsTableView
//-------------------------------------------------

const QTableView &TableViewManager::parentAsTableView() const
{
    return *dynamic_cast<const QTableView *>(QObject::parent());
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
	col_prefs.reserve(m_desc.m_columns.size());

	// get all info about each column
	for (int logicalColumn = 0; logicalColumn < m_desc.m_columns.size(); logicalColumn++)
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


