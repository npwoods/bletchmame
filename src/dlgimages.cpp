/***************************************************************************

    dlgimages.cpp

    Devices dialog

***************************************************************************/

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


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class ImagesDialog : public wxDialog
	{
	public:
		ImagesDialog(const std::vector<Image> &images, std::shared_ptr<RunMachineTask> &&run_machine_task);

	private:
		enum
		{
			// user IDs
			ID_LOAD_IMAGE = wxID_HIGHEST + 1,
			ID_UNLOAD_IMAGE,
			ID_LAST
		};

		std::shared_ptr<RunMachineTask>	m_run_machine_task;
		wxMenu							m_popup_menu;
		int								m_popup_menu_result;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxSizer &sizer, int flags, TArgs&&... args);

		void AppendToPopupMenu(int id, const wxString &text);

		bool ImageMenu(const wxButton &button, const wxString &tag);
		bool LoadImage(const wxString &tag);
		bool UnloadImage(const wxString &tag);
		void Issue(const std::initializer_list<wxString> &args);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

ImagesDialog::ImagesDialog(const std::vector<Image> &images, std::shared_ptr<RunMachineTask> &&run_machine_task)
	: wxDialog(nullptr, wxID_ANY, "Images", wxDefaultPosition, wxSize(400, 300))
	, m_run_machine_task(std::move(run_machine_task))
{
	int id = ID_LAST;
	
	// main grid
	auto grid_sizer = std::make_unique<wxFlexGridSizer>(3);
	grid_sizer->AddGrowableCol(1);
	for (const Image &image : images)
	{
		wxStaticText &static_text	= AddControl<wxStaticText>	(*grid_sizer, wxALL,			id++, image.m_tag);
		wxTextCtrl &text_ctrl		= AddControl<wxTextCtrl>	(*grid_sizer, wxALL | wxEXPAND,	id++, image.m_file_name, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
		wxButton &image_button		= AddControl<wxButton>		(*grid_sizer, wxALL,			id++, "...", wxDefaultPosition, wxSize(20, 20));

		wxString tag = image.m_tag;
		Bind(wxEVT_BUTTON, [this, &image_button, tag](auto &) { ImageMenu(image_button, tag); }, image_button.GetId());

		(void)static_text;
		(void)text_ctrl;
	}

	// buttons
	auto button_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	AddControl<wxButton>(*button_sizer, wxALL, wxID_OK, wxT("OK"));

	// overall layout
	auto sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	sizer->Add(grid_sizer.release(), 1, wxALL | wxEXPAND);
	sizer->Add(button_sizer.release(), 1, wxALL | wxALIGN_RIGHT);
	SetSizer(sizer.release());

	// popup menu
	AppendToPopupMenu(ID_LOAD_IMAGE, "Load...");
	AppendToPopupMenu(ID_UNLOAD_IMAGE, "Unload");
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
//  AppendToPopupMenu
//-------------------------------------------------

void ImagesDialog::AppendToPopupMenu(int id, const wxString &text)
{
	m_popup_menu.Append(id, text);
	Bind(wxEVT_MENU, [this, id](auto &) { m_popup_menu_result = id; }, id);
}


//-------------------------------------------------
//  ImageMenu
//-------------------------------------------------

bool ImagesDialog::ImageMenu(const wxButton &button, const wxString &tag)
{
	// display the popup menu
	wxRect button_rectangle = button.GetRect();
	m_popup_menu_result = 0;
	if (!PopupMenu(&m_popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom()))
		return false;

	// interpret the result
	bool result = false;
	switch (m_popup_menu_result)
	{
	case ID_LOAD_IMAGE:
		result = LoadImage(tag);
		break;

	case ID_UNLOAD_IMAGE:
		result = UnloadImage(tag);
		break;
	}
	return false;
}


//-------------------------------------------------
//  LoadImage
//-------------------------------------------------

bool ImagesDialog::LoadImage(const wxString &tag)
{
	wxFileDialog dialog(this, wxFileSelectorPromptStr, wxEmptyString, wxEmptyString, wxFileSelectorDefaultWildcardStr, wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (dialog.ShowModal() != wxID_OK)
		return false;
	
	Issue({ "load", tag, dialog.GetPath() });
	return true;
}


//-------------------------------------------------
//  UnloadImage
//-------------------------------------------------

bool ImagesDialog::UnloadImage(const wxString &tag)
{
	Issue({ "unload", tag });
	return false;
}


//-------------------------------------------------
//  Issue
//-------------------------------------------------

void ImagesDialog::Issue(const std::initializer_list<wxString> &args)
{
	m_run_machine_task->Issue(args);
}


//-------------------------------------------------
//  show_images_dialog
//-------------------------------------------------

bool show_images_dialog(const std::vector<Image> &images, std::shared_ptr<RunMachineTask> run_machine_task)
{
	ImagesDialog dialog(images, std::move(run_machine_task));
	return dialog.ShowModal() == wxID_OK;
}
