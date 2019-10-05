/***************************************************************************

	collectionlistview.cpp

	Subclass of wxListView, providing rich views on collections

***************************************************************************/

#include "collectionlistview.h"
#include "utility.h"
#include "prefs.h"


//-------------------------------------------------
//  GetStyle
//-------------------------------------------------

static long GetStyle(bool support_label_edit)
{
	// determine the style
	long style = wxLC_REPORT | wxLC_VIRTUAL | wxLC_SINGLE_SEL;
	if (support_label_edit)
		style |= wxLC_EDIT_LABELS;
	return style;
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

CollectionListView::CollectionListView(wxWindow &parent, wxWindowID winid, Preferences &prefs, const CollectionViewDesc &desc, std::unique_ptr<ICollectionImpl> &&coll_impl, bool support_label_edit)
	: wxListView(&parent, winid, wxDefaultPosition, wxDefaultSize, GetStyle(support_label_edit))
	, m_desc(desc)
	, m_prefs(prefs)
	, m_coll_impl(std::move(coll_impl))
	, m_key_column_index(-1)
	, m_sort_column(0)
	, m_sort_type(ColumnPrefs::sort_type::ASCENDING)
{
	// get preferences
	const std::unordered_map<std::string, ColumnPrefs> &col_prefs = m_prefs.GetColumnPrefs(desc.m_name);

	// prepare an array for order
	std::vector<int> order;
	order.resize(desc.m_columns.size());

	// build the columns
	ClearAll();
	for (int i = 0; i < desc.m_columns.size(); i++)
	{
		if (!strcmp(desc.m_columns[i].m_id, desc.m_key_column))
			m_key_column_index = i;

		// defaults
		int width = desc.m_columns[i].m_default_width;
		order[i] = i;

		// look up this column
		auto iter = col_prefs.find(desc.m_columns[i].m_id);
		if (iter != col_prefs.end())
		{
			// figure out the width
			if (iter->second.m_width > 0)
				width = iter->second.m_width;

			// figure out the order
			order[i] = iter->second.m_order;

			// is there a sort?
			if (iter->second.m_sort.has_value())
			{
				m_sort_column = i;
				m_sort_type = iter->second.m_sort.value();
			}
		}

		// append the actual column
		AppendColumn(desc.m_columns[i].m_description, wxLIST_FORMAT_LEFT, width);
	}

	// sanity checks
	assert(m_key_column_index >= 0);

	// set the column order
	SetColumnsOrder(wxArrayInt(order.begin(), order.end()));

	// bind events
	Bind(wxEVT_LIST_COL_END_DRAG,	[this](auto &)		{ UpdateColumnPrefs();					});
	Bind(wxEVT_LIST_COL_CLICK,		[this](auto &event)	{ ToggleColumnSort(event.GetColumn());	});
	Bind(wxEVT_LIST_ITEM_SELECTED,	[this](auto &event) { OnListItemSelected(event.GetIndex());	});
}


//-------------------------------------------------
//  UpdateMachineList
//-------------------------------------------------

void CollectionListView::UpdateListView()
{
	// get basic info about things
	long collection_size = m_coll_impl->GetSize();
	int column_count = GetColumnCount();

	// rebuild the indirection list
	m_indirections.clear();
	m_indirections.reserve(collection_size);
	for (long i = 0; i < collection_size; i++)
	{
		// check for a match
		bool match = m_filter_text.empty();
		for (int column = 0; !match && (column < column_count); column++)
		{
			wxString cell_text = GetActualItemText(i, column);
			match = util::string_icontains(cell_text, m_filter_text);
		}

		if (match)
			m_indirections.push_back(i);
	}

	// sort the indirection list
	std::sort(m_indirections.begin(), m_indirections.end(), [this, column_count](int a, int b)
	{
		// first compare the actual sort
		int rc = CompareActualRows(a, b, m_sort_column, m_sort_type);
		if (rc != 0)
			return rc < 0;

		// in the unlikely event of a tie, try comparing the other columns
		for (int column_index = 0; rc == 0 && column_index < column_count; column_index++)
		{
			if (column_index != m_sort_column)
				rc = CompareActualRows(a, b, column_index, ColumnPrefs::sort_type::ASCENDING);
		}
		return rc < 0;
	});

	// set the list view size
	SetItemCount(m_indirections.size());
	RefreshItems(0, m_indirections.size() - 1);

	// restore the selection
	const wxString &selected_item = GetListViewSelection();
	const int *selected_actual_index = nullptr;
	if (!selected_item.empty())
	{
		selected_actual_index = util::find_if_ptr(m_indirections, [this, &selected_item](int actual_index)
		{
			return selected_item == GetActualItemText(actual_index, m_key_column_index);
		});
	}
	Select(selected_actual_index ? selected_actual_index - &m_indirections[0] : -1);
	if (selected_actual_index)
		EnsureVisible(selected_actual_index - &m_indirections[0]);
}


//-------------------------------------------------
//  CompareActualRows
//-------------------------------------------------

int CollectionListView::CompareActualRows(int row_a, int row_b, int sort_column, ColumnPrefs::sort_type sort_type) const
{
	const wxString &a_string = GetActualItemText(row_a, sort_column);
	const wxString &b_string = GetActualItemText(row_b, sort_column);
	int compare_result = util::string_icompare(a_string, b_string);
	return sort_type == ColumnPrefs::sort_type::ASCENDING
		? compare_result
		: -compare_result;
}


//-------------------------------------------------
//  SetFilterText
//-------------------------------------------------

void CollectionListView::SetFilterText(wxString &&filter_text)
{
	if (m_filter_text != filter_text)
	{
		m_filter_text = std::move(filter_text);
		UpdateListView();
	}
}


//-------------------------------------------------
//  OnGetItemText
//-------------------------------------------------

wxString CollectionListView::OnGetItemText(long item, long column) const
{
	long actual_item = m_indirections[item];
	return GetActualItemText(actual_item, column);
}


//-------------------------------------------------
//  GetActualItemText
//-------------------------------------------------

void CollectionListView::OnListItemSelected(long item)
{
	long actual_item = m_indirections[item];
	const wxString &val = GetActualItemText(actual_item, m_key_column_index);
	SetListViewSelection(val);
}


//-------------------------------------------------
//  GetActualItemText
//-------------------------------------------------

const wxString &CollectionListView::GetActualItemText(long item, long column) const
{
	return m_coll_impl->GetItemText(item, column);
}


//-------------------------------------------------
//  UpdateColumnPrefs
//-------------------------------------------------

void CollectionListView::UpdateColumnPrefs()
{
	// get the column count
	int column_count = GetColumnCount();

	// start preparing column prefs
	std::unordered_map<std::string, ColumnPrefs> col_prefs;
	col_prefs.reserve(column_count);

	// get all info about each column
	wxArrayInt order = GetColumnsOrder();
	for (int i = 0; i < column_count; i++)
	{
		ColumnPrefs &this_col_pref = col_prefs[m_desc.m_columns[i].m_id];
		this_col_pref.m_width = GetColumnWidth(i);
		this_col_pref.m_order = order[i];
		this_col_pref.m_sort = m_sort_column == i ? m_sort_type : std::optional<ColumnPrefs::sort_type>();
	}

	// and save it
	m_prefs.SetColumnPrefs(m_desc.m_name, std::move(col_prefs));
}


//-------------------------------------------------
//  ToggleColumnSort
//-------------------------------------------------

void CollectionListView::ToggleColumnSort(int column_index)
{
	if (m_sort_column != column_index)
	{
		m_sort_column = column_index;
		m_sort_type = ColumnPrefs::sort_type::ASCENDING;
	}
	else
	{
		m_sort_type = m_sort_type == ColumnPrefs::sort_type::ASCENDING
			? ColumnPrefs::sort_type::DESCENDING
			: ColumnPrefs::sort_type::ASCENDING;
	}
	UpdateListView();
}


//-------------------------------------------------
//  GetListViewSelection
//-------------------------------------------------

const wxString &CollectionListView::GetListViewSelection() const
{
	return m_prefs.GetListViewSelection(m_desc.m_name, util::g_empty_string);
}


//-------------------------------------------------
//  SetListViewSelection
//-------------------------------------------------

void CollectionListView::SetListViewSelection(const wxString &selection)
{
	m_prefs.SetListViewSelection(m_desc.m_name, util::g_empty_string, wxString(selection));
}
