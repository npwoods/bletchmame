/***************************************************************************

    dialogs/images.cpp

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

#include "dialogs/images.h"
#include "dialogs/choosesw.h"
#include "listxmltask.h"
#include "runmachinetask.h"
#include "utility.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

// software list support is not fully baked yet
#define HAS_SOFTWARE_LISTS	0


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
			ID_GRID_CONTROLS = wxID_HIGHEST + 1,
			ID_LAST
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
		observable::unique_subscription	m_images_event_subscription;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxSizer &sizer, int flags, TArgs&&... args);

		bool ImageMenu(const wxButton &button, const wxString &tag, bool is_createable, bool is_unloadable);
		std::vector<SoftwareAndPart> GetSoftwareListParts(const wxString &tag);
		bool CreateImage(const wxString &tag);
		bool LoadImage(const wxString &tag);
		bool LoadSoftwareListPart(const wxString &tag, const std::vector<SoftwareAndPart> &parts);
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
	std::unique_ptr<wxBoxSizer> bottom_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	bottom_sizer->Add(0, 0, 1, wxRIGHT, 350);
	wxSizer *button_sizer = CreateButtonSizer(has_cancel_button ? (wxOK | wxCANCEL) : wxOK);
	if (button_sizer)
	{
		m_ok_button = dynamic_cast<wxButton *>(FindWindow(wxID_OK));
		bottom_sizer->Add(button_sizer, 0, wxALL);
	}

	// overall layout
	std::unique_ptr<wxBoxSizer> main_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	main_sizer->Add(m_grid_sizer, 1, wxALL | wxEXPAND);
	main_sizer->Add(bottom_sizer.release(), 0, wxALL | wxALIGN_RIGHT, 10);
	SetSizer(main_sizer.release());

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
	const std::vector<status::image> &images(m_host.GetImages().get());

	// iterate through the vector of images
	for (int i = 0; i < (int)images.size(); i++)
	{
		int id = ID_GRID_CONTROLS + (i * COLUMN_COUNT);
		wxStaticText *static_text;
		wxTextCtrl *text_ctrl;
		wxButton *image_button;

		// identify the tag (and drop the first colon)
		assert(!images[i].m_tag.empty());

		// identify the label and file name
		const wxString &label = images[i].m_instance_name;
		const wxString &file_name = images[i].m_file_name;

		// do we have to create new rows?
		if (m_grid_sizer->GetRows() <= i)
		{
			// we do - add controls
			static_text		= &AddControl<wxStaticText>	(*m_grid_sizer, wxALL,				id + IDOFFSET_STATIC, label);
			text_ctrl		= &AddControl<wxTextCtrl>	(*m_grid_sizer, wxALL | wxEXPAND,	id + IDOFFSET_TEXT, file_name, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
			image_button	= &AddControl<wxButton>		(*m_grid_sizer, wxALL,				id + IDOFFSET_BUTTON, "...", wxDefaultPosition, wxSize(20, 20));

			static_text->SetToolTip(images[i].m_tag);

			bool is_creatable = images[i].m_is_creatable;
			bool is_unloadable = !images[i].m_file_name.empty();
			Bind(wxEVT_BUTTON, [this, image_button, tag{images[i].m_tag}, is_creatable, is_unloadable](auto &) { ImageMenu(*image_button, tag, is_creatable, is_unloadable); }, image_button->GetId());
		}
		else
		{
			// reuse existing controls
			static_text = dynamic_cast<wxStaticText *>(FindWindowById(id + IDOFFSET_STATIC));
			text_ctrl = dynamic_cast<wxTextCtrl *>(FindWindowById(id + IDOFFSET_TEXT));
			static_text->SetLabel(label);
			text_ctrl->SetLabel(file_name);
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

bool ImagesDialog::ImageMenu(const wxButton &button, const wxString &tag, bool is_creatable, bool is_unloadable)
{
	enum
	{
		// popup menu IDs
		ID_CREATE_IMAGE = ID_LAST + 1000,
		ID_LOAD_IMAGE,
		ID_LOAD_SOFTWARE_PART,
		ID_UNLOAD,
		ID_RECENT_FILES
	};

	// get software list parts that can be loaded (if enabled)
	std::vector<SoftwareAndPart> parts;
	if (HAS_SOFTWARE_LISTS)
		parts = GetSoftwareListParts(tag);

	// setup popup menu
	MenuWithResult popup_menu;
	if (is_creatable)
		popup_menu.Append(ID_CREATE_IMAGE, "Create...");
	popup_menu.Append(ID_LOAD_IMAGE, HAS_SOFTWARE_LISTS ? "Load Image..." : "Load...");
	if (!parts.empty())
		popup_menu.Append(ID_LOAD_SOFTWARE_PART, "Load Software List Part...");
	popup_menu.Append(ID_UNLOAD, "Unload");
	popup_menu.Enable(ID_UNLOAD, is_unloadable);

	// recent files
	const std::vector<wxString> &recent_files = m_host.GetRecentFiles(tag);
	if (!recent_files.empty())
	{
		popup_menu.AppendSeparator();

		int id = ID_RECENT_FILES;
		for (const wxString &recent_file : recent_files)
		{
			wxString recent_file_basename, recent_file_extension;
			wxFileName::SplitPath(recent_file, nullptr, &recent_file_basename, &recent_file_extension);
			popup_menu.Append(id++, recent_file_basename + "." + recent_file_extension);
		}
	}

	// display the popup menu
	wxRect button_rectangle = button.GetRect();
	if (!PopupMenu(&popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom()))
		return false;

	// interpret the result
	bool result = false;
	switch (popup_menu.Result())
	{
	case ID_CREATE_IMAGE:
		result = CreateImage(tag);
		break;

	case ID_LOAD_IMAGE:
		result = LoadImage(tag);
		break;

	case ID_LOAD_SOFTWARE_PART:
		result = LoadSoftwareListPart(tag, parts);
		break;

	case ID_UNLOAD:
		result = UnloadImage(tag);
		break;

	default:
		if (popup_menu.Result() >= ID_RECENT_FILES && popup_menu.Result() < ID_RECENT_FILES + recent_files.size())
		{
			m_host.LoadImage(tag, wxString(recent_files[popup_menu.Result() - ID_RECENT_FILES]));
			result = true;
		}
		break;
	}

	return false;
}


//-------------------------------------------------
//  GetSoftwareListParts
//-------------------------------------------------

std::vector<SoftwareAndPart> ImagesDialog::GetSoftwareListParts(const wxString &tag)
{
	std::vector<SoftwareAndPart> results;

	// find the device
	auto iter = std::find_if(
		m_host.GetMachine().devices().cbegin(),
		m_host.GetMachine().devices().end(),
		[&tag](info::device dev) { return tag == dev.tag(); });
	if (iter != m_host.GetMachine().devices().end())
	{
		const wxString &dev_interface = (*iter).devinterface();

		// check software lists
		const std::vector<software_list> &softlists = m_host.GetSoftwareLists();
		for (const software_list &softlist : softlists)
		{
			for (const software_list::software &software : softlist.get_software())
			{
				for (const software_list::part &part : software.m_parts)
				{
					if (dev_interface == part.m_interface)
						results.emplace_back(softlist, software, part);
				}
			}
		}
	}

	return results;
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
//  LoadSoftwareListPart
//-------------------------------------------------

bool ImagesDialog::LoadSoftwareListPart(const wxString &tag, const std::vector<SoftwareAndPart> &parts)
{
	std::optional<int> rc = show_choose_software_dialog(*this, m_host.GetPreferences(), parts);
	if (rc.has_value())
	{
		wxString part_name = wxString::Format("%s:%s", parts[rc.value()].software().m_name, parts[rc.value()].part().m_name);
		m_host.LoadImage(tag, std::move(part_name));
	}
	return rc.has_value();
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
