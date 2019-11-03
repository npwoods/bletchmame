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

SoftwareListView::SoftwareListView(wxWindow &parent, wxWindowID winid, Preferences &prefs)
	: CollectionListView(
		parent,
		winid,
		prefs,
		s_view_desc,
		[this](long item, long column) -> const wxString &{ return GetListItemText(m_parts[item].software(), column); },
		[this]() { return m_parts.size(); },
		false)
{
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

void SoftwareListView::Load(const software_list_collection &software_col, bool load_parts, const wxString &dev_interface)
{
	// clear things out
	Clear();

	// now enumerate through each list and build the m_parts vector
	for (const software_list &softlist : software_col.software_lists())
	{
		// if the name of this software list is not in m_softlist_names, add it
		if (std::find(m_softlist_names.begin(), m_softlist_names.end(), softlist.name()) == m_softlist_names.end())
			m_softlist_names.push_back(softlist.name());

		// now enumerate through all software
		for (const software_list::software &software : softlist.get_software())
		{
			if (load_parts)
			{
				// we're loading individual parts; enumerate through them and add them
				for (const software_list::part &part : software.m_parts)
				{
					if (dev_interface.empty() || dev_interface == part.m_interface)
						m_parts.emplace_back(softlist, software, &part);
				}
			}
			else
			{
				// we're not loading individual parts
				assert(dev_interface.empty());
				m_parts.emplace_back(softlist, software, nullptr);
			}
		}
	}
}


//-------------------------------------------------
//  Clear
//-------------------------------------------------

void SoftwareListView::Clear()
{
	m_parts.clear();
	m_softlist_names.clear();
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


//-------------------------------------------------
//  GetSelectedSoftwareAndPart
//-------------------------------------------------

const SoftwareListView::SoftwareAndPart *SoftwareListView::GetSelectedSoftwareAndPart() const
{
	const SoftwareAndPart *result = nullptr;

	long selected_item = GetFirstSelected();
	if (selected_item >= 0)
	{
		int actual_selected_item = GetActualIndex(selected_item);
		result = &m_parts[actual_selected_item];
	}
	return result;
}


//-------------------------------------------------
//  GetSelectedItem
//-------------------------------------------------

wxString SoftwareListView::GetSelectedItem() const
{
	wxString result;
	const SoftwareAndPart *sp = GetSelectedSoftwareAndPart();

	if (sp)
	{
		result = sp->has_part()
			? wxString::Format("%s:%s", sp->software().m_name, sp->part().m_name)
			: sp->software().m_name;
	}
	return result;
}


//-------------------------------------------------
//  GetSelectedSoftware
//-------------------------------------------------

const software_list::software *SoftwareListView::GetSelectedSoftware() const
{
	const SoftwareAndPart *sp = GetSelectedSoftwareAndPart();
	return sp ? &sp->software() : nullptr;
}
