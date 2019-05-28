/***************************************************************************

    dlgpaths.cpp

    Paths dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/radiobut.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/listctrl.h>

#include "dlgpaths.h"
#include "prefs.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class PathsDialog : public wxDialog
	{
	public:
		PathsDialog(Preferences &prefs);

	private:
		static const size_t PATH_COUNT = (size_t)Preferences::path_type::count;
		static const std::array<wxString, PATH_COUNT> s_combo_box_strings;

		Preferences &						m_prefs;
		std::array<wxString, PATH_COUNT>	m_paths;
		wxComboBox *						m_combo_box;
		wxListView *						m_list_view;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxBoxSizer *sizer, TArgs&&... args);
		static std::array<wxString, PATH_COUNT> BuildComboBoxStrings();

		void PopulateListView();
		static bool IsMultiPath(Preferences::path_type type);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const std::array<wxString, PathsDialog::PATH_COUNT> PathsDialog::s_combo_box_strings = PathsDialog::BuildComboBoxStrings();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

PathsDialog::PathsDialog(Preferences &prefs)
	: wxDialog(nullptr, wxID_ANY, "Paths", wxDefaultPosition, wxSize(400, 300))
	, m_prefs(prefs)
	, m_combo_box(nullptr)
	, m_list_view(nullptr)
{
	int id = wxID_HIGHEST + 1;

	// path data
	for (size_t i = 0; i < PATH_COUNT; i++)
		m_paths[i] = m_prefs.GetPath(static_cast<Preferences::path_type>(i));

	// Left column
	wxBoxSizer *vbox_left = new wxBoxSizer(wxVERTICAL);
	AddControl<wxStaticText>(vbox_left, id++, "Show Paths For:");
	m_combo_box = &AddControl<wxComboBox>(
		vbox_left,
		id++,
		wxEmptyString,
		wxDefaultPosition,
		wxDefaultSize,
		s_combo_box_strings.size(),
		s_combo_box_strings.data(),
		wxCB_READONLY);
	AddControl<wxStaticText>(vbox_left, id++, "Directories:");
	m_list_view = &AddControl<wxListView>(vbox_left, id++, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER);

	// Right column
	wxBoxSizer *vbox_right = new wxBoxSizer(wxVERTICAL);
	wxButton &ok_button		= AddControl<wxButton>(vbox_right, id++, "OK");
	wxButton &close_button	= AddControl<wxButton>(vbox_right, id++, "Close");
	wxButton &browse_button = AddControl<wxButton>(vbox_right, id++, "Browse");
	wxButton &insert_button = AddControl<wxButton>(vbox_right, id++, "Insert");
	wxButton &delete_button	= AddControl<wxButton>(vbox_right, id++, "Delete");

	// Combo box
	m_combo_box->Select(0);
	PopulateListView();

	// Overall layout
	wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(vbox_left, 1);
	hbox->Add(vbox_right, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 10);
	SetSizer(hbox);

	// bind events
	Bind(wxEVT_COMBOBOX, [this](auto &) { PopulateListView(); });

	(void)ok_button;
	(void)close_button;
	(void)browse_button;
	(void)insert_button;
	(void)delete_button;
}


//-------------------------------------------------
//  PopulateListView
//-------------------------------------------------

void PathsDialog::PopulateListView()
{
	int type = m_combo_box->GetSelection();
	wxListItem item;
	int id = 0;

	m_list_view->ClearAll();
	m_list_view->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, m_list_view->GetSize().GetWidth());

	if (!m_paths[type].IsEmpty())
	{
		for (auto path : util::string_split(m_paths[type], [](wchar_t ch) { return ch == ';'; }))
		{
			item.SetText(path);
			item.SetId(id++);
			m_list_view->InsertItem(item);
		}
	}

	if (m_paths[type].IsEmpty() || IsMultiPath(static_cast<Preferences::path_type>(type)))
	{
		item.SetText("<               >");
		item.SetId(id++);
		m_list_view->InsertItem(item);
	}
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &PathsDialog::AddControl(wxBoxSizer *sizer, TArgs&&... args)
{
	TControl *control = new TControl(this, std::forward<TArgs>(args)...);
	sizer->Add(control, 0, wxTOP | wxRIGHT | wxLEFT | wxBOTTOM, 4);
	return *control;
}


//-------------------------------------------------
//  BuildComboBoxStrings
//-------------------------------------------------

std::array<wxString, PathsDialog::PATH_COUNT> PathsDialog::BuildComboBoxStrings()
{
	std::array<wxString, PATH_COUNT> result;
	result[(size_t)Preferences::path_type::emu_exectuable]	= "MAME Executable";
	result[(size_t)Preferences::path_type::roms]			= "ROMs";
	result[(size_t)Preferences::path_type::samples]			= "Samples";
	result[(size_t)Preferences::path_type::config]			= "Config Files";
	result[(size_t)Preferences::path_type::nvram]			= "NVRAM Files";
	return result;
}


//-------------------------------------------------
//  IsMultiPath
//-------------------------------------------------

bool PathsDialog::IsMultiPath(Preferences::path_type type)
{
	bool result;
	switch (type)
	{
	case Preferences::path_type::emu_exectuable:
	case Preferences::path_type::config:
	case Preferences::path_type::nvram:
		result = false;
		break;

	case Preferences::path_type::roms:
	case Preferences::path_type::samples:
		result = true;
		break;

	default:
		throw false;
	}
	return result;
}


//-------------------------------------------------
//  show_paths_dialog
//-------------------------------------------------

bool show_paths_dialog(Preferences prefs)
{
	PathsDialog dlg(prefs);
	return dlg.ShowModal() == wxID_OK;
}
