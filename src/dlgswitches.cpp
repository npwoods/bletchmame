/***************************************************************************

    dlgswitches.h

    Configuration & DIP Switches customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/scrolwin.h>
#include <wx/button.h>

#include "dlgswitches.h"
#include "runmachinetask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class SwitchesDialog : public wxDialog
	{
	public:
		SwitchesDialog(wxWindow &parent, const wxString &title, ISwitchesHost &host, Input::input_class input_class, info::machine machine);

	private:
		ISwitchesHost &				m_host;
		Input::input_class			m_input_class;
		info::machine				m_machine;
		wxScrolledWindow *			m_scrolled;
		wxFlexGridSizer *			m_grid_sizer;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args);
		void UpdateInputs();
		std::unordered_map<std::uint32_t, wxString> GetChoices(const Input &input) const;
		void OnSettingChanged(wxComboBox &combo_box, const wxString &port_tag, ioport_value mask, const std::vector<std::uint32_t> &choice_values);
	};
};


enum
{
	ID_RESTORE_DEFAULTS = wxID_LAST + 1,
	ID_LAST
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

SwitchesDialog::SwitchesDialog(wxWindow &parent, const wxString &title, ISwitchesHost &host, Input::input_class input_class, info::machine machine)
	: wxDialog(&parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(host)
	, m_input_class(input_class)
	, m_machine(machine)
	, m_scrolled(nullptr)
	, m_grid_sizer(nullptr)
{
	// scrolled view
	std::unique_ptr<wxBoxSizer> outer_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	m_scrolled = &AddControl<wxScrolledWindow>(*this, *outer_sizer, 1, wxALL | wxEXPAND);

	// buttons
	std::unique_ptr<wxBoxSizer> button_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	wxButton &restore_button	= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, ID_RESTORE_DEFAULTS, "Restore Defaults");
	wxButton &ok_button			= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, wxID_OK, "OK");
	outer_sizer->Add(button_sizer.release(), 0, wxALIGN_RIGHT);
	
	// restore button is not yet implemented
	restore_button.Disable();

	// main grid
	m_grid_sizer = new wxFlexGridSizer(2);

	// configure scrolling
	m_scrolled->SetScrollRate(25, 25);
	m_scrolled->SetSizer(m_grid_sizer);

	// finally set the outer sizer
	SetSizer(outer_sizer.release());
	
	// update the inputs
	UpdateInputs();

	// appease compiler
	(void)ok_button;
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &SwitchesDialog::AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args)
{
	TControl *control = new TControl(&parent, std::forward<TArgs>(args)...);
	sizer.Add(control, proportion, flags, 4);
	return *control;
}


//-------------------------------------------------
//  UpdateInputs
//-------------------------------------------------

void SwitchesDialog::UpdateInputs()
{
	int id = ID_LAST;

	for (const Input &input : m_host.GetInputs())
	{
		if (input.m_class == m_input_class)
		{
			// get all choices
			std::unordered_map<std::uint32_t, wxString> choices = GetChoices(input);

			// get the values and strings
			std::vector<std::uint32_t> choice_values;
			std::vector<wxString> choice_strings;
			for (const auto &pair : choices)
			{
				choice_values.push_back(pair.first);
				choice_strings.push_back(pair.second);
			}

			// add static text with the name of the config item
			AddControl<wxStaticText>(*m_scrolled, *m_grid_sizer, 0, wxALL, id++, input.m_name);

			// create a combo box with the values
			wxComboBox &combo_box = AddControl<wxComboBox>(
				*m_scrolled,
				*m_grid_sizer,
				0,
				wxALL | wxEXPAND,
				id++,
				wxEmptyString,
				wxDefaultPosition,
				wxDefaultSize,
				choice_strings.size(),
				choice_strings.data(),
				wxCB_READONLY);

			// select the proper value
			auto iter = choices.find(input.m_value);
			if (iter != choices.end())
				combo_box.SetValue(iter->second);

			// bind the event
			wxString port_tag = input.m_port_tag;
			ioport_value mask = input.m_mask;
			Bind(wxEVT_COMBOBOX, [this, &combo_box, port_tag{std::move(port_tag)}, mask{std::move(mask)}, choice_values{std::move(choice_values)}](auto &) { OnSettingChanged(combo_box, port_tag, mask, choice_values); }, combo_box.GetId());
		}
	}

	m_scrolled->FitInside();
}


//-------------------------------------------------
//  GetChoices
//-------------------------------------------------

std::unordered_map<std::uint32_t, wxString> SwitchesDialog::GetChoices(const Input &input) const
{
	// get the tag, remove initial colon
	wxString tag = input.m_port_tag;
	if (tag.StartsWith(":"))
		tag = tag.substr(1);

	// find the configuration with this tag
	std::unordered_map<std::uint32_t, wxString> results;
	auto iter = std::find_if(
		m_machine.configurations().begin(),
		m_machine.configurations().end(),
		[&input, &tag](const info::configuration c)
		{
			return c.tag() == tag && c.mask() == input.m_mask;
		});

	// did we found it?
	if (iter != m_machine.configurations().end())
	{
		// we did - map its setting values
		info::configuration configuration = *iter;
		for (const info::configuration_setting setting : configuration.settings())
			results[setting.value()] = setting.name();
	}

	return results;
}


//-------------------------------------------------
//  OnSettingChanged
//-------------------------------------------------

void SwitchesDialog::OnSettingChanged(wxComboBox &combo_box, const wxString &port_tag, ioport_value mask, const std::vector<std::uint32_t> &choice_values)
{
	// get the selection index
	int index = combo_box.GetSelection();

	// get the value out of choices
	ioport_value value = choice_values[index];

	// and specify it
	m_host.SetInputValue(port_tag, mask, value);
}


//-------------------------------------------------
//  show_switches_dialog
//-------------------------------------------------

bool show_switches_dialog(wxWindow &parent, const wxString &title, ISwitchesHost &host, Input::input_class input_class, info::machine machine)
{
	SwitchesDialog dialog(parent, title, host, input_class, machine);
	return dialog.ShowModal() == wxID_OK;
}
