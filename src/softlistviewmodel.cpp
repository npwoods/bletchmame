/***************************************************************************

	softlistviewmodel.cpp

	Software list view

***************************************************************************/

#include "softlistviewmodel.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

static const CollectionViewDesc s_view_desc =
{
	SOFTLIST_VIEW_DESC_NAME,
	"name",
	{
		{ "name",			"Name",			85 },
		{ "description",	"Description",	220 },
		{ "year",			"Year",			50 },
		{ "publisher",		"Publisher",	190 }
	}
};


//-------------------------------------------------
//  ctor
//-------------------------------------------------

SoftwareListViewModel::SoftwareListViewModel(QTableView &tableView, Preferences &prefs)
	: CollectionViewModel(
		tableView,
		prefs,
		s_view_desc,
		[this](long item, long column) -> const QString &{ return GetListItemText(m_parts[item].software(), column); },
		[this]() { return m_parts.size(); },
		false)
{
}


//-------------------------------------------------
//  Load
//-------------------------------------------------

void SoftwareListViewModel::Load(const software_list_collection &software_col, bool load_parts, const QString &dev_interface)
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
					if (dev_interface.isEmpty() || dev_interface == part.m_interface)
						m_parts.emplace_back(softlist, software, &part);
				}
			}
			else
			{
				// we're not loading individual parts
				assert(dev_interface.isEmpty());
				m_parts.emplace_back(softlist, software, nullptr);
			}
		}
	}
}


//-------------------------------------------------
//  Clear
//-------------------------------------------------

void SoftwareListViewModel::Clear()
{
	m_parts.clear();
	m_softlist_names.clear();
}


//-------------------------------------------------
//  GetListItemText
//-------------------------------------------------

const QString &SoftwareListViewModel::GetListItemText(const software_list::software &sw, long column)
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

const QString &SoftwareListViewModel::getListViewSelection() const
{
	for (const QString &softlist_name : m_softlist_names)
	{
		const QString &selection = Prefs().GetListViewSelection(s_view_desc.m_name, softlist_name);
		if (!selection.isEmpty())
			return selection;
	}
	return util::g_empty_string;
}


//-------------------------------------------------
//  SetListViewSelection
//-------------------------------------------------

void SoftwareListViewModel::setListViewSelection(const QString &selection)
{
	for (const QString &softlist_name : m_softlist_names)
	{
		bool found = util::find_if_ptr(m_parts, [&softlist_name, &selection](const SoftwareAndPart &x)
		{
			return x.softlist().name() == softlist_name && x.software().m_name == selection;
		});

		QString this_selection = found ? selection : QString();
		Prefs().SetListViewSelection(s_view_desc.m_name, softlist_name, std::move(this_selection));
	}
}


//-------------------------------------------------
//  GetSelectedSoftwareAndPart
//-------------------------------------------------

const SoftwareListViewModel::SoftwareAndPart *SoftwareListViewModel::GetSelectedSoftwareAndPart() const
{
	const SoftwareAndPart *result = nullptr;

	long selected_item = getFirstSelected();
	if (selected_item >= 0)
	{
		result = &m_parts[selected_item];
	}
	return result;
}


//-------------------------------------------------
//  GetSelectedItem
//-------------------------------------------------

QString SoftwareListViewModel::GetSelectedItem() const
{
	QString result;
	const SoftwareAndPart *sp = GetSelectedSoftwareAndPart();

	if (sp)
	{
		result = sp->has_part()
			? QString("%1:%2").arg(sp->software().m_name, sp->part().m_name)
			: sp->software().m_name;
	}
	return result;
}


//-------------------------------------------------
//  GetSelectedSoftware
//-------------------------------------------------

const software_list::software *SoftwareListViewModel::GetSelectedSoftware() const
{
	const SoftwareAndPart *sp = GetSelectedSoftwareAndPart();
	return sp ? &sp->software() : nullptr;
}
