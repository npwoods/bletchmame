/***************************************************************************

    dialogs/inputs.cpp

    Input customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/button.h>

#include "dialogs/inputs.h"
#include "runmachinetask.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class InputsDialog : public wxDialog
	{
	public:
		InputsDialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class);

	private:
		IInputsHost &							m_host;
		wxDialog *								m_current_dialog;
		observable::unique_subscription			m_polling_seq_changed_subscription;
		std::unordered_map<wxString, wxString>	m_codes;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args);

		void StartInputPoll(wxButton &button, wxStaticText &static_text, const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type);
		void OnPollingSeqChanged();
		wxString GetSeqText(const status::input_seq &seq);
		static std::unordered_map<wxString, wxString> BuildCodes(const std::vector<status::input_class> &devclasses);
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::InputsDialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class)
	: wxDialog(&parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(host)
	, m_current_dialog(nullptr)
{
	int id = wxID_LAST + 1;

	// build codes map
	m_codes = BuildCodes(m_host.GetInputClasses());

	// scrolled view
	std::unique_ptr<wxBoxSizer> outer_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	wxScrolledWindow &scrolled = AddControl<wxScrolledWindow>(*this, *outer_sizer, 1, wxALL | wxEXPAND);

	// buttons
	std::unique_ptr<wxBoxSizer> button_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	wxButton &restore_button	= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, id++, "Restore Defaults");
	wxButton &ok_button			= AddControl<wxButton>(*this, *button_sizer, 0, wxALL, wxID_OK, "OK");
	outer_sizer->Add(button_sizer.release(), 0, wxALIGN_RIGHT);
	
	// restore button is not yet implemented
	restore_button.Disable();

	// main grid
	std::unique_ptr<wxFlexGridSizer> grid_sizer = std::make_unique<wxFlexGridSizer>(2);

	// build controls
	wxString buffer;
	for (const status::input &input : host.GetInputs())
	{
		if (input.m_class == input_class)
		{
			for (const status::input_seq &input_seq : input.m_seqs)
			{
				const wxString *name;
				switch (input_seq.m_type)
				{
				case status::input_seq::type::STANDARD:
					name = &input.m_name;
					break;
				case status::input_seq::type::INCREMENT:
					buffer = input.m_name + " Inc";
					name = &buffer;
					break;
				case status::input_seq::type::DECREMENT:
					buffer = input.m_name + " Dec";
					name = &buffer;
					break;
				default:
					throw false;
				}

				wxButton &button			= AddControl<wxButton>(scrolled, *grid_sizer, 0, wxALL | wxEXPAND, id++, *name);
				wxStaticText &static_text	= AddControl<wxStaticText>(scrolled, *grid_sizer, 0, wxALL, id++, GetSeqText(input_seq));

				wxString port_tag = input.m_port_tag;
				int mask = input.m_mask;
				status::input_seq::type seq_type = input_seq.m_type;
				Bind(wxEVT_BUTTON, [this, &button, &static_text, port_tag, mask, seq_type](auto &) { StartInputPoll(button, static_text, port_tag, mask, seq_type); }, button.GetId());
			}
		}
	}

	// configure scrolling
	scrolled.SetScrollRate(25, 25);
	scrolled.SetSizer(grid_sizer.release());
	scrolled.FitInside();

	// finally set the outer sizer
	SetSizer(outer_sizer.release());

	// observe
	m_polling_seq_changed_subscription = m_host.GetPollingSeqChanged().subscribe([this]() { OnPollingSeqChanged(); });

	// appease compiler
	(void)ok_button;
}


//-------------------------------------------------
//  AddControl
//-------------------------------------------------

template<typename TControl, typename... TArgs>
TControl &InputsDialog::AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args)
{
	TControl *control = new TControl(&parent, std::forward<TArgs>(args)...);
	sizer.Add(control, proportion, flags, 4);
	return *control;
}


//-------------------------------------------------
//  StartInputPoll
//-------------------------------------------------

void InputsDialog::StartInputPoll(wxButton &button, wxStaticText &static_text, const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type)
{
	// start polling
	m_host.StartPolling(port_tag, mask, seq_type);

	// present the dialog
	wxDialog dialog(this, wxID_ANY, "Press key for " + button.GetLabel(), wxDefaultPosition, wxSize(250, 100));
	m_current_dialog = &dialog;
	dialog.ShowModal();
	m_current_dialog = nullptr;

	// stop polling (though this might have happened implicitly)
	m_host.StopPolling();

	// update the text (by now we expect the text to have been updated)
	const std::vector<status::input> &inputs(m_host.GetInputs());
	auto input_iter = std::find_if(inputs.begin(), inputs.end(), [&port_tag, mask](const status::input &input)
	{
		return input.m_port_tag == port_tag && input.m_mask == mask;
	});
	if (input_iter != inputs.end())
	{
		auto seq_iter = std::find_if(input_iter->m_seqs.begin(), input_iter->m_seqs.end(), [seq_type](const status::input_seq &seq)
		{
			return seq.m_type == seq_type;
		});
		if (seq_iter != input_iter->m_seqs.end())
		{
			wxString seq_text = GetSeqText(*seq_iter);
			static_text.SetLabel(seq_text);
		}
	}
}


//-------------------------------------------------
//  OnPollingSeqChanged
//-------------------------------------------------

void InputsDialog::OnPollingSeqChanged()
{
	if (!m_host.GetPollingSeqChanged().get() && m_current_dialog)
		m_current_dialog->Close();
}


//-------------------------------------------------
//  BuildCodes
//-------------------------------------------------

std::unordered_map<wxString, wxString> InputsDialog::BuildCodes(const std::vector<status::input_class> &devclasses)
{
	std::unordered_map<wxString, wxString> result;
	for (const status::input_class &devclass : devclasses)
	{
		if (devclass.m_enabled)
		{
			// similar logic to devclass_string_table in MAME input.cpp
			wxString devclass_name;
			if (devclass.m_name == "keyboard")
				devclass_name = devclass.m_devices.size() > 1 ? "Kbd" : "";
			else if (devclass.m_name == "joystick")
				devclass_name = "Joy";
			else if (devclass.m_name == "lightgun")
				devclass_name = "Gun";
			else
				devclass_name = devclass.m_name;

			// build codes for each device
			for (const status::input_device &dev : devclass.m_devices)
			{
				wxString prefix = !devclass_name.empty()
					? wxString::Format("%s #%d ", devclass_name, dev.m_index + 1)
					: wxString();

				for (const status::input_device_item &item : dev.m_items)
				{
					wxString label = prefix + item.m_name;
					result.emplace(item.m_code, std::move(label));
				}
			}
		}
	}
	return result;
}


//-------------------------------------------------
//  GetSeqText
//-------------------------------------------------

wxString InputsDialog::GetSeqText(const status::input_seq &seq)
{
	// this replicates logic in core MAME; need to more fully build this out, and perhaps
	// more fully dissect input sequences
	wxString result;
	std::vector<wxString> tokens = util::string_split(seq.m_tokens, [](wchar_t ch) { return ch == ' '; });
	for (const wxString &token : tokens)
	{
		wxString word;
		if (token == "OR" || token == "NOT" || token == "DEFAULT")
		{
			word = token.Lower();
		}
		else
		{
			auto iter = m_codes.find(token);
			if (iter != m_codes.end())
				word = iter->second;
		}

		if (!word.empty())
		{
			if (!result.empty())
				result += " ";
			result += word;
		}
	}

	// quick and dirty hack to trim out "OR" tokens at the start and end
	while (result.StartsWith("or "))
		result = result.substr(3);
	while (result.EndsWith(" or"))
		result = result.substr(0, result.size() - 3);
	if (result == "or")
		result = "";

	// show something at least
	if (result.empty())
		result = "None";

	return result;
}


//-------------------------------------------------
//  show_inputs_dialog
//-------------------------------------------------

bool show_inputs_dialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class)
{
	InputsDialog dialog(parent, title, host, input_class);
	return dialog.ShowModal() == wxID_OK;
}
