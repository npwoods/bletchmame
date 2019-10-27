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
	// ======================> ChooseSoftlistPartDialog
	class ChooseSoftlistPartDialog : public wxDialog
	{
	public:
		ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts);

		const std::optional<int> &Selection() const { return m_selection; }

		void UpdateColumnPrefs()
		{
			m_list_view->UpdateColumnPrefs();
		}

	private:
		Preferences &							m_prefs;
		std::optional<int>						m_selection;
		wxTextCtrl *							m_search_box;
		CollectionListView *					m_list_view;
		wxButton *								m_ok_button;

		void OnSelectionChanged();
		void OnSearchBoxTextChanged();
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ChooseSoftlistPartDialog::ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts)
	: wxDialog(&parent, wxID_ANY, wxT("Choose Software List Part"), wxDefaultPosition, wxSize(600, 400), wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_prefs(prefs)
	, m_search_box(nullptr)
	, m_list_view(nullptr)
	, m_ok_button(nullptr)
{
	int id = wxID_LAST + 1;

	// create the search box
	const wxString &search_box_text = prefs.GetSearchBoxText(SOFTLIST_VIEW_DESC_NAME);
	m_search_box = new wxTextCtrl(this, id++, search_box_text);

	// create a list view
	m_list_view = new SoftwareListView(*this, id++, prefs, parts);
	m_list_view->UpdateListView();

	// bind events
	Bind(wxEVT_LIST_ITEM_SELECTED,	[this](auto &) { OnSelectionChanged();		}, m_list_view->GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,	[this](auto &) { EndDialog(wxID_OK);		}, m_list_view->GetId());
	Bind(wxEVT_TEXT,				[this](auto &) { OnSearchBoxTextChanged();	}, m_search_box->GetId());

	// specify the sizer
	SpecifySizer(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 0, wxRIGHT | wxLEFT | wxEXPAND,	*m_search_box },
		{ 1, wxALL | wxEXPAND,				*m_list_view },
		{ 0, wxALL | wxALIGN_RIGHT,			CreateButtonSizer(wxOK | wxCANCEL) }
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
//  OnSearchBoxTextChanged
//-------------------------------------------------

void ChooseSoftlistPartDialog::OnSearchBoxTextChanged()
{
	m_prefs.SetSearchBoxText(SOFTLIST_VIEW_DESC_NAME, m_search_box->GetValue());
	m_list_view->UpdateListView();
}


//-------------------------------------------------
//  show_choose_software_dialog
//-------------------------------------------------

std::optional<int> show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts)
{
	ChooseSoftlistPartDialog dialog(parent, prefs, parts);
	int rc = dialog.ShowModal();
	dialog.UpdateColumnPrefs();
	return rc == wxID_OK ? dialog.Selection() : std::optional<int>();
}
