/***************************************************************************

	dialogs/choosesw.cpp

	Choose Software dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/button.h>

#include "dialogs/choosesw.h"
#include "virtuallistview.h"
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
		ChooseSoftlistPartDialog(wxWindow &parent, const std::vector<SoftwareAndPart> &parts);

		const std::optional<int> &Selection() const { return m_selection; }

	private:
		std::optional<int>	m_selection;
		wxListView *		m_list_view;
		wxButton *			m_ok_button;

		void OnSelectionChanged();
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ChooseSoftlistPartDialog::ChooseSoftlistPartDialog(wxWindow &parent, const std::vector<SoftwareAndPart> &parts)
	: wxDialog(&parent, wxID_ANY, wxT("Choose Software List Part"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_list_view(nullptr)
	, m_ok_button(nullptr)
{
	// create a list view
	VirtualListView &list_view = *new VirtualListView(this, wxID_ANY);
	list_view.AppendColumn(wxT("Name"));
	list_view.AppendColumn(wxT("Description"));
	list_view.AppendColumn(wxT("Publisher"));
	list_view.AppendColumn(wxT("Year"));
	list_view.SetItemCount(parts.size());
	list_view.SetOnGetItemText([&parts](long item, long column)
	{
		switch(column)
		{
			case 0:	return parts[item].software().m_name;
			case 1:	return parts[item].software().m_description;
			case 2:	return parts[item].software().m_publisher;
			case 3:	return parts[item].software().m_year;
		}
		throw false;
	});

	// bind events
	Bind(wxEVT_LIST_ITEM_SELECTED,	[this](auto &) { OnSelectionChanged();	}, list_view.GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,	[this](auto &) { EndDialog(wxID_OK);	}, list_view.GetId());

	// specify the sizer
	SpecifySizer(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 1, wxALL | wxEXPAND,		list_view },
		{ 0, wxALL | wxALIGN_RIGHT,	CreateButtonSizer(wxOK | wxCANCEL) }
	} });

	m_list_view = &list_view;
	m_ok_button = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	OnSelectionChanged();
}


//-------------------------------------------------
//  OnSelectionChanged
//-------------------------------------------------

void ChooseSoftlistPartDialog::OnSelectionChanged()
{
	long first_selected = m_list_view->GetFirstSelected();
	m_selection = first_selected >= 0 ? (int)first_selected : std::optional<int>();

	if (m_ok_button)
		m_ok_button->Enable(m_selection.has_value());
}


//-------------------------------------------------
//  show_choose_software_dialog
//-------------------------------------------------

std::optional<int> show_choose_software_dialog(wxWindow &parent, const std::vector<SoftwareAndPart> &parts)
{
	ChooseSoftlistPartDialog dialog(parent, parts);
	int rc = dialog.ShowModal();
	return rc == wxID_OK ? dialog.Selection() : std::optional<int>();
}
