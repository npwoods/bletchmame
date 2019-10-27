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
		ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const software_list_collection &software_col, const wxString &dev_interface);

		wxString Selection() const { return m_list_view->GetSelectedItem(); }

		void UpdateColumnPrefs()
		{
			m_list_view->UpdateColumnPrefs();
		}

	private:
		Preferences &							m_prefs;
		wxTextCtrl *							m_search_box;
		SoftwareListView *						m_list_view;
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

ChooseSoftlistPartDialog::ChooseSoftlistPartDialog(wxWindow &parent, Preferences &prefs, const software_list_collection &software_col, const wxString &dev_interface)
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
	m_list_view = new SoftwareListView(*this, id++, prefs);
	m_list_view->Load(software_col, true, dev_interface);
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
	bool has_selection = m_list_view->GetFirstSelected() >= 0;
	if (m_ok_button)
		m_ok_button->Enable(has_selection);
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

wxString show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const software_list_collection &software_col, const wxString &dev_interface)
{
	ChooseSoftlistPartDialog dialog(parent, prefs, software_col, dev_interface);
	int rc = dialog.ShowModal();
	dialog.UpdateColumnPrefs();
	return rc == wxID_OK ? dialog.Selection() : wxString();
}
