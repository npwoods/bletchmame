/***************************************************************************

    dialogs/inputs.cpp

    Input customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/button.h>

#include <list>

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
		class Entry
		{
		public:
			Entry(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, wxButton &button, wxStaticText &static_text, bool is_analog);

			const wxString &		PortTag() const { return m_port_tag; }
			ioport_value			Mask() const { return m_mask; }
			status::input_seq::type	SeqType() const { return m_seq_type; }
			wxButton &				Button() { return m_button;}
			wxStaticText &			StaticText() { return m_static_text; }
			bool					IsAnalog() const { return m_is_analog; }

		private:
			wxString				m_port_tag;
			ioport_value			m_mask;
			status::input_seq::type	m_seq_type;
			wxButton &				m_button;
			wxStaticText &			m_static_text;
			bool					m_is_analog;
		};

		struct DeviceItemRef
		{
			DeviceItemRef(const status::input_class &devclass, const status::input_device &dev, const status::input_device_item &item)
				: m_devclass(devclass)
				, m_dev(dev)
				, m_item(item)
			{
			}

			const status::input_class &			m_devclass;
			const status::input_device &		m_dev;
			const status::input_device_item &	m_item;
		};

		IInputsHost &							m_host;
		wxDialog *								m_current_dialog;
		observable::unique_subscription			m_inputs_subscription;
		observable::unique_subscription			m_polling_seq_changed_subscription;
		std::unordered_map<wxString, wxString>	m_codes;
		std::list<Entry>						m_entries;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args);

		bool ShowPopupMenu(Entry &entry);
		std::list<DeviceItemRef> SuggestAnalogInputDeviceItems() const;
		const status::input_seq *FindInputSeq(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type);
		void StartInputPoll(const wxString &label, const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, const wxString &start_seq);
		void OnInputsChanged();
		void OnPollingSeqChanged();
		wxString GetSeqTextFromTokens(const wxString &seq_tokens);
		static std::unordered_map<wxString, wxString> BuildCodes(const std::vector<status::input_class> &devclasses);
		static wxString GetDeviceClassName(const status::input_class &devclass, bool hide_single_keyboard);
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
	for (const status::input &input : host.GetInputs().get())
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
				bool is_analog = input.m_type == status::input::input_type::ANALOG
					&& input_seq.m_type == status::input_seq::type::STANDARD;

				// create the controls
				wxButton &button			= AddControl<wxButton>(scrolled, *grid_sizer, 0, wxALL | wxEXPAND, id++, *name);
				wxStaticText &static_text	= AddControl<wxStaticText>(scrolled, *grid_sizer, 0, wxALL, id++, GetSeqTextFromTokens(input_seq.m_tokens));

				// create the entry
				Entry &entry = m_entries.emplace_back(
					input.m_port_tag,
					input.m_mask,
					input_seq.m_type,
					button,
					static_text,
					is_analog);

				// bind the menu item
				Bind(wxEVT_BUTTON, [this, &entry](auto &) { ShowPopupMenu(entry); }, entry.Button().GetId());
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
	m_inputs_subscription = m_host.GetInputs().subscribe([this]() { OnInputsChanged(); });
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
//  ShowPopupMenu
//-------------------------------------------------

bool InputsDialog::ShowPopupMenu(Entry &entry)
{
	enum
	{
		ID_SPECIFY_INPUT = wxID_HIGHEST + 1,
		ID_ADD_INPUT,
		ID_QUICK_ITEMS_BEGIN,
		ID_CLEAR_INPUT = ID_QUICK_ITEMS_BEGIN
	};

	// keep a list of so-called "quick" items; direct specifications of input seqs
	std::vector<wxString> quick_items = { "" };

	// create the pop up menu
	MenuWithResult popup_menu;
	if (entry.IsAnalog())
	{
		std::list<DeviceItemRef> items = SuggestAnalogInputDeviceItems();
		if (!items.empty())
		{
			for (const DeviceItemRef &ref : items)
			{
				// put this quick item away
				int id = ID_QUICK_ITEMS_BEGIN + (int)quick_items.size();
				quick_items.push_back(ref.m_item.m_code);

				// identify the label for the quick item
				wxString label = wxString::Format("%s #%d %s (%s)",
					GetDeviceClassName(ref.m_devclass, false),
					ref.m_dev.m_index + 1,
					ref.m_item.m_name,
					ref.m_dev.m_name);

				// and append
				popup_menu.Append(id, label);
			}

			// finally append a separator
			popup_menu.AppendSeparator();
		}
	}
	popup_menu.Append(ID_SPECIFY_INPUT, "Specify...");
	popup_menu.Append(ID_ADD_INPUT, "Add...");
	popup_menu.Append(ID_CLEAR_INPUT, "Clear");

	// show the pop up menu
	wxRect button_rectangle = entry.Button().GetRect();
	if (!PopupMenu(&popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom()))
		return false;

	switch (popup_menu.Result())
	{
	case ID_SPECIFY_INPUT:
	case ID_ADD_INPUT:
		{
			// if we're adding (and not specifying), we need to find the input seq that
			// we're appending to; this shouldn't fail unless something very odd is going
			// on but so be it
			const status::input_seq *append_to_seq = popup_menu.Result() == ID_ADD_INPUT
				? FindInputSeq(entry.PortTag(), entry.Mask(), entry.SeqType())
				: nullptr;

			// based on that, identify the sequence of tokens that we're starting on
			const wxString &start_seq_tokens = append_to_seq
				? append_to_seq->m_tokens
				: util::g_empty_string;

			// and start polling!
			StartInputPoll(entry.Button().GetLabel(), entry.PortTag(), entry.Mask(), entry.SeqType(), start_seq_tokens);
		}
		break;

	default:
		if (popup_menu.Result() >= ID_QUICK_ITEMS_BEGIN && popup_menu.Result() < ID_QUICK_ITEMS_BEGIN + quick_items.size())
		{
			int index = popup_menu.Result() - ID_QUICK_ITEMS_BEGIN;
			const wxString &seq_tokens = quick_items[index];
			m_host.SetInputSeq(entry.PortTag(), entry.Mask(), entry.SeqType(), seq_tokens);
		}
		break;
	}

	return true;
}


//-------------------------------------------------
//  SuggestAnalogInputDeviceItems
//-------------------------------------------------

std::list<InputsDialog::DeviceItemRef> InputsDialog::SuggestAnalogInputDeviceItems() const
{
	std::list<DeviceItemRef> results;

	// build the results
	const std::vector<status::input_class> &devclasses = m_host.GetInputClasses();
	for (const status::input_class &devclass : devclasses)
	{
		for (const status::input_device &dev : devclass.m_devices)
		{
			for (const status::input_device_item &item : dev.m_items)
			{
				if (item.m_token == "XAXIS" || item.m_token == "YAXIS" || item.m_token == "ZAXIS")
					results.emplace_back(devclass, dev, item);
			}
		}
	}

	// some postprocessing; remove lightgun devices that "echo" mouses; perhaps this is a specific
	// example of a more general problem?
	decltype(results)::iterator iter;
	bool done = false;
	while(!done)
	{
		iter = std::find_if(results.begin(), results.end(), [&results](const DeviceItemRef &x)
		{
			return (x.m_devclass.m_name == wxT("lightgun"))
				&& util::find_if_ptr(results, [&results, &x](const DeviceItemRef &y)
				{
					return y.m_devclass.m_name == wxT("mouse")
						&& x.m_dev.m_name == y.m_dev.m_name;
				});
		});
		if (iter != results.end())
			results.erase(iter);
		else
			done = true;
	}

	return results;
}


//-------------------------------------------------
//  FindInputSeq
//-------------------------------------------------

const status::input_seq *InputsDialog::FindInputSeq(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type)
{
	// look up the input port by tag and mask
	const status::input *input = util::find_if_ptr(m_host.GetInputs().get(), [&port_tag, mask](const status::input &x)
	{
		return x.m_port_tag == port_tag && x.m_mask == mask;
	});
	if (!input)
		return nullptr;

	// and once we have that, look up the seq
	return util::find_if_ptr(input->m_seqs, [seq_type](const status::input_seq &x)
	{
		return x.m_type == seq_type;
	});
}


//-------------------------------------------------
//  StartInputPoll
//-------------------------------------------------

void InputsDialog::StartInputPoll(const wxString &label, const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, const wxString &start_seq)
{
	// start polling
	m_host.StartPolling(port_tag, mask, seq_type, start_seq);

	// present the dialog
	wxDialog dialog(this, wxID_ANY, "Press key for " + label, wxDefaultPosition, wxSize(250, 100));
	m_current_dialog = &dialog;
	dialog.ShowModal();
	m_current_dialog = nullptr;

	// stop polling (though this might have happened implicitly)
	m_host.StopPolling();
}


//-------------------------------------------------
//  OnInputsChanged
//-------------------------------------------------

void InputsDialog::OnInputsChanged()
{
	for (Entry &entry : m_entries)
	{
		const status::input_seq *seq = FindInputSeq(entry.PortTag(), entry.Mask(), entry.SeqType());
		if (seq)
		{
			wxString label = GetSeqTextFromTokens(seq->m_tokens);
			entry.StaticText().SetLabel(label);
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
		// similar logic to devclass_string_table in MAME input.cpp
		wxString devclass_name = GetDeviceClassName(devclass, true);

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
	return result;
}


//-------------------------------------------------
//  GetDeviceClassName
//-------------------------------------------------

wxString InputsDialog::GetDeviceClassName(const status::input_class &devclass, bool hide_single_keyboard)
{
	wxString result;
	if (devclass.m_name == "keyboard")
		result = !hide_single_keyboard || devclass.m_devices.size() > 1 ? "Kbd" : "";
	else if (devclass.m_name == "joystick")
		result = "Joy";
	else if (devclass.m_name == "lightgun")
		result = "Gun";
	else if (devclass.m_name == "mouse")
		result = "Mouse";
	else
		result = devclass.m_name;
	return result;
}


//-------------------------------------------------
//  GetSeqTextFromTokens
//-------------------------------------------------

wxString InputsDialog::GetSeqTextFromTokens(const wxString &seq_tokens)
{
	// this replicates logic in core MAME; need to more fully build this out, and perhaps
	// more fully dissect input sequences
	wxString result;
	std::vector<wxString> tokens = util::string_split(seq_tokens, [](wchar_t ch) { return ch == ' '; });
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
//  Entry ctor
//-------------------------------------------------

InputsDialog::Entry::Entry(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, wxButton &button, wxStaticText &static_text, bool is_analog)
	: m_port_tag(port_tag)
	, m_mask(mask)
	, m_seq_type(seq_type)
	, m_button(button)
	, m_static_text(static_text)
	, m_is_analog(is_analog)
{
}


//-------------------------------------------------
//  show_inputs_dialog
//-------------------------------------------------

bool show_inputs_dialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class)
{
	InputsDialog dialog(parent, title, host, input_class);
	return dialog.ShowModal() == wxID_OK;
}
