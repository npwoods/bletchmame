/***************************************************************************

	dialogs/choosesw.cpp

	Choose Software dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/button.h>

#include "dialogs/choosesw.h"
#include "collectionlistview.h"
#include "utility.h"
#include "wxhelpers.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ChooseSoftlistPartDialog : public wxDialog
	{
	public:
		ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts);

		const std::optional<int> &Selection() const { return m_selection; }

	private:
		const std::vector<SoftwareAndPart> &	m_parts;
		std::optional<int>						m_selection;
		CollectionListView *					m_list_view;
		wxButton *								m_ok_button;

		void OnSelectionChanged();
		const wxString &GetListItemText(const software_list::software &sw, long column);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

static const CollectionViewDesc s_view_desc =
{
	"softlist",
	{
		{ "name",			wxT("Name"),			85 },
		{ "description",	wxT("Description"),		370 },
		{ "year",			wxT("Year"),			50 },
		{ "publisher",		wxT("Publisher"),		320 }
	}
};


//-------------------------------------------------
//  ctor
//-------------------------------------------------

ChooseSoftlistPartDialog::ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts)
	: wxDialog(&parent, wxID_ANY, wxT("Choose Software List Part"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_parts(parts)
	, m_list_view(nullptr)
	, m_ok_button(nullptr)
{
	// create a list view
	m_list_view = new CollectionListView(
		*this,
		wxID_ANY,
		prefs,
		s_view_desc,
		[this](long item, long column) -> const wxString &	{ return GetListItemText(m_parts[item].software(), column); },
		[this]()											{ return m_parts.size(); },
		false);
	m_list_view->UpdateListView();

	// bind events
	Bind(wxEVT_LIST_ITEM_SELECTED,	[this](auto &) { OnSelectionChanged();	}, m_list_view->GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,	[this](auto &) { EndDialog(wxID_OK);	}, m_list_view->GetId());

	// specify the sizer
	SpecifySizer(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 1, wxALL | wxEXPAND,		*m_list_view },
		{ 0, wxALL | wxALIGN_RIGHT,	CreateButtonSizer(wxOK | wxCANCEL) }
	} });

	m_ok_button = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	OnSelectionChanged();
}


//-------------------------------------------------
//  OnSelectionChanged
//-------------------------------------------------

void ChooseSoftlistPartDialog::OnSelectionChanged()
{
	long first_selected = m_list_view->GetFirstSelected();
	m_selection = first_selected >= 0
		? m_list_view->GetActualIndex(first_selected)
		: std::optional<int>();

	if (m_ok_button)
		m_ok_button->Enable(m_selection.has_value());
}


//-------------------------------------------------
//  show_choose_software_dialog
//-------------------------------------------------

const wxString &ChooseSoftlistPartDialog::GetListItemText(const software_list::software &sw, long column)
{
	switch (column)
	{
	case 0:	return sw.m_name;
	case 1:	return sw.m_description;
	case 2:	return sw.m_publisher;
	case 3:	return sw.m_year;
	}
	throw false;
}


//-------------------------------------------------
//  show_choose_software_dialog
//-------------------------------------------------

std::optional<int> show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts)
{
	ChooseSoftlistPartDialog dialog(parent, prefs, parts);
	int rc = dialog.ShowModal();
	return rc == wxID_OK ? dialog.Selection() : std::optional<int>();
}
