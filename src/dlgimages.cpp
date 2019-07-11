/***************************************************************************

    dlgimages.cpp

    Devices dialog

***************************************************************************/

#include <wx/filename.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/filedlg.h>

#include "dlgimages.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ImagesDialog : public wxDialog
	{
	public:
		ImagesDialog(IImagesHost &host, bool has_cancel_button);

	private:
		enum
		{
			// user IDs
			ID_CREATE_IMAGE = wxID_HIGHEST + 1,
			ID_LOAD_IMAGE,
			ID_UNLOAD_IMAGE,
			ID_GRID_CONTROLS,
			ID_RECENT_FILES
		};

		enum
		{
			IDOFFSET_STATIC,
			IDOFFSET_TEXT,
			IDOFFSET_BUTTON
		};

		static const int COLUMN_COUNT = 3;

		IImagesHost &					m_host;
		wxFlexGridSizer *				m_grid_sizer;
		wxButton *						m_ok_button;
		int								m_popup_menu_result;
		observable::unique_subscription	m_images_event_subscription;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxSizer &sizer, int flags, TArgs&&... args);

		void AppendToPopupMenu(wxMenu &popup_menu, int id, const wxString &text);

		bool ImageMenu(const wxButton &button, const wxString &tag, bool is_createable, bool is_unloadable);
		bool CreateImage(const wxString &tag);
		bool LoadImage(const wxString &tag);
		bool UnloadImage(const wxString &tag);
		wxString GetWildcardString(const wxString &tag, bool support_zip) const;
		void UpdateImageGrid();
		void UpdateWorkingDirectory(const wxString &path);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImagesDialog::ImagesDialog(IImagesHost &host, bool has_cancel_button)
	: wxDialog(nullptr, wxID_ANY, "Images", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(host)
	, m_grid_sizer(nullptr)
	, m_ok_button(nullptr)
{
	// host interactions
	m_images_event_subscription = m_host.GetImages().subscribe([this] { UpdateImageGrid(); });

	// main grid
	m_grid_sizer = new wxFlexGridSizer(COLUMN_COUNT);
	m_grid_sizer->AddGrowableCol(1);

	// buttons
	wxSizer *button_sizer = CreateButtonSizer(has_cancel_button ? (wxOK | wxCANCEL) : wxOK);
	if (button_sizer)
		m_ok_button = dynamic_cast<wxButton *>(FindWindow(wxID_OK));

	// overall layout
	wxBoxSizer *main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(m_grid_sizer, 1, wxALL | wxEXPAND);
	if (button_sizer)
		main_sizer->Add(button_sizer, 0, wxALL | wxALIGN_RIGHT, 10);
	SetSizer(main_sizer);

	// initial update of image grid
	UpdateImageGrid();

	// now figure out how big we are
	Fit();
}


//-------------------------------------------------
//  UpdateImageGrid
//-------------------------------------------------

void ImagesDialog::UpdateImageGrid()
{
	bool ok_enabled = true;

	// get the list of images
	const std::vector<Image> &images(m_host.GetImages().get());

	// iterate through the vector of images
	for (int i = 0; i < (int)images.size(); i++)
	{
		int id = ID_GRID_CONTROLS + (i * COLUMN_COUNT);
		wxStaticText *static_text;
		wxTextCtrl *text_ctrl;
		wxButton *image_button;

		// identify the tag (and drop the first colon)
		assert(!images[i].m_tag.IsEmpty());
		wxString tag = images[i].m_tag[0] == ':'
			? images[i].m_tag.SubString(1, images[i].m_tag.size() - 1)
			: images[i].m_tag;

		// do we have to create new rows?
		if (m_grid_sizer->GetRows() <= i)
		{
			// we do - add controls
			static_text		= &AddControl<wxStaticText>	(*m_grid_sizer, wxALL,				id + IDOFFSET_STATIC, tag);
			text_ctrl		= &AddControl<wxTextCtrl>	(*m_grid_sizer, wxALL | wxEXPAND,	id + IDOFFSET_TEXT, images[i].m_file_name, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
			image_button	= &AddControl<wxButton>		(*m_grid_sizer, wxALL,				id + IDOFFSET_BUTTON, "...", wxDefaultPosition, wxSize(20, 20));

			bool is_createable = images[i].m_is_createable;
			bool is_unloadable = !images[i].m_file_name.empty();
			Bind(wxEVT_BUTTON, [this, image_button, tag, is_createable, is_unloadable](auto &) { ImageMenu(*image_button, tag, is_createable, is_unloadable); }, image_button->GetId());
		}
		else
		{
			// reuse existing controls
			static_text = dynamic_cast<wxStaticText *>(FindWindowById(id + IDOFFSET_STATIC));
			text_ctrl = dynamic_cast<wxTextCtrl *>(FindWindowById(id + IDOFFSET_TEXT));
			static_text->SetLabel(tag);
			text_ctrl->SetLabel(images[i].m_file_name);
		}

		// if this is an image that must be loaded, make it black and disable "ok"
		const wxColour *static_text_color = wxBLACK;
		if (images[i].m_must_be_loaded && images[i].m_file_name.IsEmpty())
		{
			static_text_color = wxRED;
			ok_enabled = false;
		}
		if (static_text->GetForegroundColour() != *static_text_color)
		{
			// refreshing should not be necessary, but c'est la vie
			static_text->SetForegroundColour(*static_text_color);
			static_text->Refresh();
		}
	}

	// remove extra controls
	for (int i = ID_GRID_CONTROLS + images.size() * COLUMN_COUNT; i < ID_GRID_CONTROLS + m_grid_sizer->GetRows() * COLUMN_COUNT; i++)
	{
		FindWindowById(i)->Destroy();
	}

	// update the sizer's row count
	m_grid_sizer->SetRows(images.size());

	// update the "OK" button
	m_ok_button->Enable(ok_enabled);
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &ImagesDialog::AddControl(wxSizer &sizer, int flags, TArgs&&... args)
{
	TControl *control = new TControl(this, std::forward<TArgs>(args)...);
	sizer.Add(control, 0, flags, 4);
	return *control;
}


//-------------------------------------------------
//  ImageMenu
//-------------------------------------------------

bool ImagesDialog::ImageMenu(const wxButton &button, const wxString &tag, bool is_createable, bool is_unloadable)
{
	wxMenu popup_menu;

	// setup popup menu
	if (is_createable)
		AppendToPopupMenu(popup_menu, ID_CREATE_IMAGE, "Create...");
	AppendToPopupMenu(popup_menu, ID_LOAD_IMAGE, "Load...");
	AppendToPopupMenu(popup_menu, ID_UNLOAD_IMAGE, "Unload");
	popup_menu.Enable(ID_UNLOAD_IMAGE, is_unloadable);

	// recent files
	const std::vector<wxString> &recent_files = m_host.GetRecentFiles(tag);
	if (!recent_files.empty())
	{
		popup_menu.AppendSeparator();

		int id = ID_RECENT_FILES;
		for (const wxString &recent_file : recent_files)
			AppendToPopupMenu(popup_menu, id++, recent_file);
	}

	// display the popup menu
	wxRect button_rectangle = button.GetRect();
	m_popup_menu_result = 0;
	if (!PopupMenu(&popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom()))
		return false;

	// interpret the result
	bool result = false;
	switch (m_popup_menu_result)
	{
	case ID_CREATE_IMAGE:
		result = CreateImage(tag);
		break;

	case ID_LOAD_IMAGE:
		result = LoadImage(tag);
		break;

	case ID_UNLOAD_IMAGE:
		result = UnloadImage(tag);
		break;

	default:
		if (m_popup_menu_result >= ID_RECENT_FILES && m_popup_menu_result < ID_RECENT_FILES + recent_files.size())
		{
			m_host.LoadImage(tag, wxString(recent_files[m_popup_menu_result - ID_RECENT_FILES]));
			result = true;
		}
		break;
	}

	return false;
}


//-------------------------------------------------
//  AppendToPopupMenu
//-------------------------------------------------

void ImagesDialog::AppendToPopupMenu(wxMenu &popup_menu, int id, const wxString &text)
{
	popup_menu.Append(id, text);
	Bind(wxEVT_MENU, [this, id](auto &) { m_popup_menu_result = id; }, id);
}


//-------------------------------------------------
//  CreateImage
//-------------------------------------------------

bool ImagesDialog::CreateImage(const wxString &tag)
{
	// show the dialog
	wxFileDialog dialog(
		this,
		wxFileSelectorPromptStr,
		m_host.GetWorkingDirectory(),
		wxEmptyString,
		GetWildcardString(tag, false),
		wxFD_SAVE);
	if (dialog.ShowModal() != wxID_OK)
		return false;

	// get the result from the dialog
	wxString path = dialog.GetPath();

	// update our host's working directory
	UpdateWorkingDirectory(path);

	// and load the image
	m_host.CreateImage(tag, std::move(path));
	return true;
}



//-------------------------------------------------
//  LoadImage
//-------------------------------------------------

bool ImagesDialog::LoadImage(const wxString &tag)
{
	// show the dialog
	wxFileDialog dialog(
		this,
		wxFileSelectorPromptStr,
		m_host.GetWorkingDirectory(),
		wxEmptyString,
		GetWildcardString(tag, true),
		wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (dialog.ShowModal() != wxID_OK)
		return false;

	// get the result from the dialog
	wxString path = dialog.GetPath();

	// update our host's working directory
	UpdateWorkingDirectory(path);

	// and load the image
	m_host.LoadImage(tag, std::move(path));
	return true;
}


//-------------------------------------------------
//  UnloadImage
//-------------------------------------------------

bool ImagesDialog::UnloadImage(const wxString &tag)
{
	m_host.UnloadImage(tag);
	return false;
}


//-------------------------------------------------
//  GetWildcardString
//-------------------------------------------------

wxString ImagesDialog::GetWildcardString(const wxString &tag, bool support_zip) const
{
	// get the list of extensions
	std::vector<wxString> extensions = m_host.GetExtensions(tag);

	// append zip if appropriate
	if (support_zip)
		extensions.push_back("zip");

	// figure out the "general" wildcard part for all devices
	wxString all_extensions = util::string_join(wxString(";"), extensions, [](wxString ext) { return wxString::Format("*.%s", ext); });
	wxString result = wxString::Format("Device files (%s)|%s", all_extensions, all_extensions);

	// now break out each extension
	for (const wxString &ext : extensions)
	{
		result += wxString::Format("|%s files (*.%s)|*.%s",
			ext.Upper(),
			ext,
			ext);
	}

	// and all files
	result += "|All files (*.*)|*.*";
	return result;
}


//-------------------------------------------------
//  UpdateWorkingDirectory
//-------------------------------------------------

void ImagesDialog::UpdateWorkingDirectory(const wxString &path)
{
	wxString dir;
	wxFileName::SplitPath(path, &dir, nullptr, nullptr);
	m_host.SetWorkingDirectory(std::move(dir));
}


//-------------------------------------------------
//  show_images_dialog
//-------------------------------------------------

void show_images_dialog(IImagesHost &host)
{
	ImagesDialog dialog(host, false);
	dialog.ShowModal();
}


//-------------------------------------------------
//  show_images_dialog_cancellable
//-------------------------------------------------

bool show_images_dialog_cancellable(IImagesHost &host)
{
	ImagesDialog dialog(host, true);
	return dialog.ShowModal() == wxID_OK;
}
