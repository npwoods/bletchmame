/***************************************************************************

	softlistview.h

	Software list view

***************************************************************************/

#pragma once

#ifndef SOFTLISTVIEW_H
#define SOFTLISTVIEW_H

#include "softwarelist.h"
#include "collectionlistview.h"

#define SOFTLIST_VIEW_DESC_NAME "softlist"


// ======================> SoftwareAndPart
class SoftwareAndPart
{
public:
	SoftwareAndPart(const software_list &sl, const software_list::software &sw, const software_list::part &p)
		: m_softlist(sl)
		, m_software(sw)
		, m_part(p)
	{
	}

	const software_list &softlist() const { return m_softlist; }
	const software_list::software &software() const { return m_software; }
	const software_list::part &part() const { return m_part; }

private:
	const software_list &m_softlist;
	const software_list::software &m_software;
	const software_list::part &m_part;
};


// ======================> SoftwareListView
class SoftwareListView : public CollectionListView
{
public:
	SoftwareListView(wxWindow &parent, wxWindowID winid, Preferences &prefs, const std::vector<SoftwareAndPart> &parts);

protected:
	virtual const wxString &GetListViewSelection() const override;
	virtual void SetListViewSelection(const wxString &selection) override;

private:
	const std::vector<SoftwareAndPart> &m_parts;
	std::vector<wxString>					m_softlist_names;

	const wxString &GetListItemText(const software_list::software &sw, long column);
};


#endif // SOFTLISTVIEW_H
