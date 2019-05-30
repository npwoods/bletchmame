/***************************************************************************

    dlgpaths.cpp

    Paths dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/listctrl.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/dir.h>
#include <wx/dirdlg.h>

#include "dlgpaths.h"
#include "prefs.h"
#include "utility.h"
#include "virtuallistview.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class PathsDialog : public wxDialog
	{
	public:
		PathsDialog(Preferences &prefs);

		void Persist();

	private:
		enum
		{
			// user IDs
			ID_NEW_DIR_ENTRY = wxID_HIGHEST + 1,
			ID_LAST
		};

		static const size_t PATH_COUNT = (size_t)Preferences::path_type::count;
		static const std::array<wxString, PATH_COUNT> s_combo_box_strings;

		Preferences &						m_prefs;
		std::array<wxString, PATH_COUNT>	m_path_lists;
		std::vector<wxString>				m_current_path_list;
		std::vector<bool>					m_current_path_valid_list;
		wxComboBox *						m_combo_box;
		VirtualListView *					m_list_view;
		wxListItemAttr						m_list_item_attr;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxBoxSizer *sizer, TArgs&&... args);
		static std::array<wxString, PATH_COUNT> BuildComboBoxStrings();

		void UpdateCurrentPathList();
		void RefreshListView();
		void CurrentPathListChanged();
		bool IsMultiPath() const;
		bool IsSelectingPath() const;
		Preferences::path_type GetCurrentPath() const;
		bool BrowseForPath();
		bool BrowseForPath(long item);
		bool BrowseForPath(size_t item);
		void OnListBeginLabelEdit(long item);
		void OnListEndLabelEdit(long item);
		void OnInsert();
		void OnDelete();
		void SetPathValue(size_t item, wxString &&value);
		wxString GetListItemText(size_t item) const;
		wxListItemAttr *GetListItemAttr(size_t item);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const std::array<wxString, PathsDialog::PATH_COUNT> PathsDialog::s_combo_box_strings = BuildComboBoxStrings();


//-------------------------------------------------
//  ctor
//-------------------------------------------------

PathsDialog::PathsDialog(Preferences &prefs)
	: wxDialog(nullptr, wxID_ANY, "Paths", wxDefaultPosition, wxSize(400, 300))
	, m_prefs(prefs)
	, m_combo_box(nullptr)
	, m_list_view(nullptr)
{
	int id = ID_LAST;

	// path data
	for (size_t i = 0; i < PATH_COUNT; i++)
		m_path_lists[i] = m_prefs.GetPath(static_cast<Preferences::path_type>(i));

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
	m_list_view = &AddControl<VirtualListView>(vbox_left, id++, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_NO_HEADER | wxLC_EDIT_LABELS | wxLC_VIRTUAL);
	m_list_view->SetOnGetItemText([this](long item, long)	{ return GetListItemText(static_cast<size_t>(item)); });
	m_list_view->SetOnGetItemAttr([this](long item)			{ return GetListItemAttr(static_cast<size_t>(item)); });

	// Right column
	wxBoxSizer *vbox_right = new wxBoxSizer(wxVERTICAL);
	wxButton &ok_button		= AddControl<wxButton>(vbox_right, wxID_OK,		"OK");
	wxButton &cancel_button = AddControl<wxButton>(vbox_right, wxID_CANCEL,	"Cancel");
	wxButton &browse_button = AddControl<wxButton>(vbox_right, id++,		"Browse");
	wxButton &insert_button = AddControl<wxButton>(vbox_right, id++,		"Insert");
	wxButton &delete_button	= AddControl<wxButton>(vbox_right, id++,		"Delete");

	// Combo box
	m_combo_box->Select(0);

	// List view
	m_list_view->ClearAll();
	m_list_view->AppendColumn(wxEmptyString, wxLIST_FORMAT_LEFT, m_list_view->GetSize().GetWidth());
	UpdateCurrentPathList();	

	// Overall layout
	wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);
	hbox->Add(vbox_left, 1);
	hbox->Add(vbox_right, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 10);
	SetSizer(hbox);

	// bind events
	Bind(wxEVT_COMBOBOX,				[this](auto &)		{ UpdateCurrentPathList(); });
	Bind(wxEVT_BUTTON,					[this](auto &)		{ BrowseForPath();									}, browse_button.GetId());
	Bind(wxEVT_BUTTON,					[this](auto &)		{ OnInsert();										}, insert_button.GetId());
	Bind(wxEVT_BUTTON,					[this](auto &)		{ OnDelete();										}, delete_button.GetId());
	Bind(wxEVT_UPDATE_UI,				[this](auto &event) { event.Enable(false);								}, insert_button.GetId());
	Bind(wxEVT_UPDATE_UI,				[this](auto &event) { event.Enable(IsMultiPath() && IsSelectingPath());	}, delete_button.GetId());
	Bind(wxEVT_LIST_ITEM_ACTIVATED,		[this](auto &event) { BrowseForPath(event.GetIndex());					});
	Bind(wxEVT_LIST_BEGIN_LABEL_EDIT,	[this](auto &event) { OnListBeginLabelEdit(event.GetIndex());			});
	Bind(wxEVT_LIST_END_LABEL_EDIT,		[this](auto &event) { OnListEndLabelEdit(event.GetIndex());				});

	// appease compiler
	(void)ok_button;
	(void)cancel_button;
}


//-------------------------------------------------
//  Persist
//-------------------------------------------------

void PathsDialog::Persist()
{
	for (size_t i = 0; i < PATH_COUNT; i++)
		m_prefs.SetPath((Preferences::path_type) i, std::move(m_path_lists[i]));
}


//-------------------------------------------------
//  BrowseForPath
//-------------------------------------------------

bool PathsDialog::BrowseForPath()
{
	long item = m_list_view->GetFocusedItem();
	return BrowseForPath(static_cast<size_t>(item));
}


bool PathsDialog::BrowseForPath(long item)
{
	return BrowseForPath(static_cast<size_t>(item));
}


bool PathsDialog::BrowseForPath(size_t item)
{
	// should really be a sanity check
	if (item > m_current_path_list.size())
		return false;

	// show the file dialog
	wxString path = show_specify_single_path_dialog(*this, GetCurrentPath());
	if (path.IsEmpty())
		return false;

	// specify it
	SetPathValue(item, std::move(path));
	return true;
}


//-------------------------------------------------
//  OnListBeginLabelEdit
//-------------------------------------------------

void PathsDialog::OnListBeginLabelEdit(long item)
{
	// is this the "extension" item?  if so we need to empty out the text
	if (static_cast<size_t>(item) == m_current_path_list.size())
	{
		wxTextCtrl *edit_control = m_list_view->GetEditControl();
		edit_control->ChangeValue("");
	}
}


//-------------------------------------------------
//  OnListEndLabelEdit
//-------------------------------------------------

void PathsDialog::OnListEndLabelEdit(long item)
{
	wxTextCtrl *edit_control = m_list_view->GetEditControl();
	wxString value = edit_control->GetValue();
	SetPathValue(static_cast<size_t>(item), std::move(value));
}


//-------------------------------------------------
//  SetPathValue
//-------------------------------------------------

void PathsDialog::SetPathValue(size_t item, wxString &&value)
{
	if (item == m_current_path_list.size())
		m_current_path_list.push_back(std::move(value));
	else
		m_current_path_list[item] = std::move(value);
	CurrentPathListChanged();
}


//-------------------------------------------------
//  OnInsert
//-------------------------------------------------

void PathsDialog::OnInsert()
{
	throw false;
}


//-------------------------------------------------
//  OnDelete
//-------------------------------------------------

void PathsDialog::OnDelete()
{
	size_t item = static_cast<size_t>(m_list_view->GetFocusedItem());
	if (item >= m_current_path_list.size())
		return;

	// delete it!
	m_current_path_list.erase(m_current_path_list.begin() + item);
	CurrentPathListChanged();
}


//-------------------------------------------------
//  CurrentPathListChanged
//-------------------------------------------------

void PathsDialog::CurrentPathListChanged()
{
	// reflect changes on the m_current_path_list back into m_path_lists 
	wxString path_list = util::string_join(wxString(";"), m_current_path_list);
	m_path_lists[m_combo_box->GetSelection()] = std::move(path_list);
	
	// because the list view may have changed, we need to refresh it
	RefreshListView();
}


//-------------------------------------------------
//  GetListItemText
//-------------------------------------------------

wxString PathsDialog::GetListItemText(size_t item) const
{
	return item < m_current_path_list.size()
		? m_current_path_list[item]
		: "<               >";
}


//-------------------------------------------------
//  GetListItemAttr
//-------------------------------------------------

wxListItemAttr *PathsDialog::GetListItemAttr(size_t item)
{
	bool is_valid = item >= m_current_path_valid_list.size() || m_current_path_valid_list[item];
	m_list_item_attr.SetTextColour(is_valid ? *wxBLACK : *wxRED);
	return &m_list_item_attr;
}


//-------------------------------------------------
//  UpdateCurrentPathList
//-------------------------------------------------

void PathsDialog::UpdateCurrentPathList()
{
	int type = m_combo_box->GetSelection();

	if (m_path_lists[type].IsEmpty())
		m_current_path_list.clear();
	else
		m_current_path_list = util::string_split(m_path_lists[type], [](wchar_t ch) { return ch == ';'; });

	RefreshListView();
}


//-------------------------------------------------
//  RefreshListView
//-------------------------------------------------

void PathsDialog::RefreshListView()
{
	// recalculate m_current_path_valid_list
	m_current_path_valid_list.resize(m_current_path_list.size());
	bool expect_dir = GetCurrentPath() != Preferences::path_type::emu_exectuable;
	for (size_t i = 0; i < m_current_path_list.size(); i++)
	{
		m_current_path_valid_list[i] = expect_dir
			? wxDir::Exists(m_current_path_list[i])
			: wxFile::Exists(m_current_path_list[i]);
	}

	// update the item count and refresh all items
	long item_count = m_current_path_list.size() + (IsMultiPath() ? 1 : 0);
	m_list_view->SetItemCount(item_count);
	m_list_view->RefreshItems(0, item_count - 1);
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

bool PathsDialog::IsMultiPath() const
{
	bool result;
	switch (GetCurrentPath())
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
//  GetCurrentPath
//-------------------------------------------------

Preferences::path_type PathsDialog::GetCurrentPath() const
{
	int selection = m_combo_box->GetSelection();
	return static_cast<Preferences::path_type>(selection);
}


//-------------------------------------------------
//  IsSelectingPath
//-------------------------------------------------

bool PathsDialog::IsSelectingPath() const
{
	size_t item = static_cast<size_t>(m_list_view->GetFocusedItem());
	return item < m_current_path_list.size();
}


//-------------------------------------------------
//  show_specify_single_path_dialog
//-------------------------------------------------

wxString show_specify_single_path_dialog(wxWindow &parent, Preferences::path_type type)
{
	wxString result;
	if (type == Preferences::path_type::emu_exectuable)
	{
		wxFileDialog dialog(&parent, "Specify MAME Path", "", "", "EXE files (*.exe)|*.exe", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dialog.ShowModal() == wxID_OK)
			result = dialog.GetPath();
	}
	else
	{
		wxDirDialog dialog(&parent, "Specify Path", "");
		if (dialog.ShowModal() == wxID_OK)
			result = dialog.GetPath();
	}
	return result;
}


//-------------------------------------------------
//  show_paths_dialog
//-------------------------------------------------

bool show_paths_dialog(Preferences &prefs)
{
	PathsDialog dialog(prefs);
	bool result = dialog.ShowModal() == wxID_OK;
	if (result)
	{
		dialog.Persist();
	}
	return result;
}
