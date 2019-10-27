/***************************************************************************

	softlistview.cpp

	Software list view

***************************************************************************/

#include "softlistview.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

static const CollectionViewDesc s_view_desc =
{
	SOFTLIST_VIEW_DESC_NAME,
	"name",
	{
		{ "name",			wxT("Name"),			85 },
		{ "description",	wxT("Description"),		220 },
		{ "year",			wxT("Year"),			50 },
		{ "publisher",		wxT("Publisher"),		190 }
	}
};


//-------------------------------------------------
//  ctor
//-------------------------------------------------

SoftwareListView::SoftwareListView(wxWindow &parent, wxWindowID winid, Preferences &prefs, const std::vector<SoftwareAndPart> &parts)
	: CollectionListView(
		parent,
		winid,
		prefs,
		s_view_desc,
		[this](long item, long column) -> const wxString &{ return GetListItemText(m_parts[item].software(), column); },
		[this]() { return m_parts.size(); },
		false)
	, m_parts(parts)
{
	for (const auto &part : parts)
	{
		const wxString &name = part.softlist().name();
		if (std::find(m_softlist_names.begin(), m_softlist_names.end(), name) == m_softlist_names.end())
			m_softlist_names.push_back(name);
	}
}


//-------------------------------------------------
//  GetListItemText
//-------------------------------------------------

const wxString &SoftwareListView::GetListItemText(const software_list::software &sw, long column)
{
	switch (column)
	{
	case 0:	return sw.m_name;
	case 1:	return sw.m_description;
	case 2:	return sw.m_year;
	case 3:	return sw.m_publisher;
	}
	throw false;
}


//-------------------------------------------------
//  GetListViewSelection
//-------------------------------------------------

const wxString &SoftwareListView::GetListViewSelection() const
{
	for (const wxString &softlist_name : m_softlist_names)
	{
		const wxString &selection = Prefs().GetListViewSelection(s_view_desc.m_name, softlist_name);
		if (!selection.empty())
			return selection;
	}
	return util::g_empty_string;
}


//-------------------------------------------------
//  SetListViewSelection
//-------------------------------------------------

void SoftwareListView::SetListViewSelection(const wxString &selection)
{
	for (const wxString &softlist_name : m_softlist_names)
	{
		bool found = util::find_if_ptr(m_parts, [&softlist_name, &selection](const SoftwareAndPart &x)
		{
			return x.softlist().name() == softlist_name && x.software().m_name == selection;
		});

		wxString this_selection = found ? selection : wxString();
		Prefs().SetListViewSelection(s_view_desc.m_name, softlist_name, std::move(this_selection));
	}
}
