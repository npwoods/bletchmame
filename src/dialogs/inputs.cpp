/***************************************************************************

    dialogs/inputs.cpp

    Input customization dialog

***************************************************************************/

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/scrolwin.h>
#include <wx/button.h>
#include <wx/listctrl.h>

#include <list>

#include "dialogs/inputs.h"
#include "runmachinetask.h"
#include "virtuallistview.h"


//**************************************************************************
//  CONSTANTS
//**************************************************************************

static const wxString s_menu_item_text_specify = wxT("Specify...");
static const wxString s_menu_item_text_add = wxT("Add...");
static const wxString s_menu_item_text_multiple = wxT("Multiple...");
static const wxString s_menu_item_text_clear = wxT("Clear");


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	enum class axis_type
	{
		NONE,
		X,
		Y,
		Z
	};

	class InputsDialog;

	// ======================> InputEntryDesc
	// represents an entry as a part of the input analysis process
	struct InputEntryDesc
	{
		// ctor
		InputEntryDesc();

		// fields
		const status::input *	m_digital;
		const status::input *	m_analog_x;
		const status::input *	m_analog_y;
		wxString				m_aggregate_name;

		// methods
		const status::input &GetSingleInput() const;
	};


	// ======================> InputFieldRef
	struct InputFieldRef
	{
	public:
		wxString				m_port_tag;
		ioport_value			m_mask;

		InputFieldRef()
		{
		}

		InputFieldRef(const wxString &port_tag, ioport_value mask)
			: m_port_tag(port_tag)
			, m_mask(mask)
		{
		}

		InputFieldRef(const status::input &input)
			: InputFieldRef(input.m_port_tag, input.m_mask)
		{
		}

		bool operator==(const InputFieldRef &that) const
		{
			return m_port_tag == that.m_port_tag
				&& m_mask == that.m_mask;
		}
	};


	// ======================> InputEntry
	// abstract base class for lines/entries in the input dialog
	class InputEntry
	{
	public:
		struct QuickItem
		{
			wxString						m_label;
			std::vector<SetInputSeqRequest>	m_selections;
		};

		InputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text);
		virtual ~InputEntry() { }
		void UpdateText();
		virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() = 0;

	protected:
		// overridden by child classes
		virtual wxString GetText() = 0;
		virtual void OnMainButtonPressed() = 0;
		virtual bool OnMenuButtonPressed() = 0;

		// accessors
		InputsDialog &Host() { return m_host; }
		wxButton &MainButton() { return m_main_button; }

		// methods
		bool PopupMenu(wxMenu &popup_menu);
		std::vector<QuickItem> BuildQuickItems(const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref, const std::optional<InputFieldRef> &all_axes_field_ref);
		bool InvokeQuickItem(std::vector<InputEntry::QuickItem> &&quick_items, int index);
		bool ShowMultipleQuickItemsDialog(std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last);

	private:
		wxWindow &		m_parent;
		InputsDialog &	m_host;
		wxButton &		m_main_button;
		wxButton &		m_menu_button;
		wxStaticText &	m_static_text;
	};


	// ======================> SingularInputEntry
	class SingularInputEntry : public InputEntry
	{
	public:
		SingularInputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text, InputFieldRef &&field_ref, status::input_seq::type seq_type);

		virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() override;

	protected:
		virtual wxString GetText() override;
		virtual void OnMainButtonPressed() override;
		virtual bool OnMenuButtonPressed() override;

	private:
		InputFieldRef			m_field_ref;
		status::input_seq::type	m_seq_type;
	};


	// ======================> MultiAxisInputEntry
	class MultiAxisInputEntry : public InputEntry
	{
	public:
		MultiAxisInputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text, const status::input *x_input, const status::input *y_input);

		virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() override;

	protected:
		virtual void OnMainButtonPressed() override;
		virtual bool OnMenuButtonPressed() override;
		virtual wxString GetText() override;

	private:
		std::optional<InputFieldRef>	m_x_field_ref;
		std::optional<InputFieldRef>	m_y_field_ref;

		bool Specify();
	};


	// ======================> InputsDialog
	class InputsDialog : public wxDialog
	{
	public:
		InputsDialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class);

		// methods
		const status::input_seq &FindInputSeq(const InputFieldRef &field_ref, status::input_seq::type seq_type);
		wxString GetSeqTextFromTokens(const wxString &seq_tokens);
		void StartInputPoll(const wxString &label, const InputFieldRef &field_ref, status::input_seq::type seq_type, const wxString &start_seq = util::g_empty_string);

		// trampolines
		void SetInputSeqs(std::vector<SetInputSeqRequest> &&seqs)
		{
			m_host.SetInputSeqs(std::move(seqs));
		}
		const std::vector<status::input_class> &GetInputClasses()
		{
			return m_host.GetInputClasses();
		}
		observable::value<std::vector<status::input>> &GetInputs()
		{
			return m_host.GetInputs();
		}

		// statics
		static std::tuple<wxString, wxString> ParseIndividualToken(wxString &&token);
		static wxString GetDeviceClassName(const status::input_class &devclass, bool hide_single_keyboard);

	private:
		IInputsHost &								m_host;
		wxDialog *									m_current_dialog;
		observable::unique_subscription				m_inputs_subscription;
		observable::unique_subscription				m_polling_seq_changed_subscription;
		std::unordered_map<wxString, wxString>		m_codes;
		std::vector<std::unique_ptr<InputEntry>>	m_entries;

		template<typename TControl, typename... TArgs> TControl &AddControl(wxWindow &parent, wxSizer &sizer, int proportion, int flags, TArgs&&... args);

		void OnInputsChanged();
		void OnPollingSeqChanged();
		void OnRestoreButtonPressed();
		std::vector<InputEntryDesc> BuildInitialEntryDescriptions(status::input::input_class input_class) const;

		// statics
		static bool CompareInputs(const status::input &a, const status::input &b);
		static std::unordered_map<wxString, wxString> BuildCodes(const std::vector<status::input_class> &devclasses);
	};


	// ======================> SeqPollingDialog
	class SeqPollingDialog : public wxDialog
	{
	public:
		SeqPollingDialog(InputsDialog &host, const wxString &title_text, const wxString &instructions_text);
		wxString &DialogSelectedResult() { return m_dialog_selected_result; }

	private:
		InputsDialog &	m_host;
		wxButton *		m_mouse_inputs_button;
		wxString		m_dialog_selected_result;

		void OnMouseButtonPressed();
	};


	// ======================> MultiAxisInputDialog
	class MultiAxisInputDialog : public wxDialog
	{
	public:
		MultiAxisInputDialog(InputsDialog &parent, const wxString &title, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref);

	private:
		InputsDialog &						m_host;
		std::vector<SingularInputEntry>		m_singular_inputs;
		observable::unique_subscription		m_inputs_subscription;

		wxSizer &AddInputPanel(const std::optional<InputFieldRef> &field_ref, status::input_seq::type seq_type, int &id, wxString &&label);
		wxWindow &AddSpacer();

		InputsDialog &Host() { return m_host; }
		void OnInputsChanged();
		void SetAllInputs(const wxString &tokens);
	};


	// ======================> MultipleQuickItemsDialog
	class MultipleQuickItemsDialog : public wxDialog
	{
	public:
		MultipleQuickItemsDialog(wxWindow &parent, std::vector<InputEntry::QuickItem>::const_iterator first, std::vector<InputEntry::QuickItem>::const_iterator last);
		std::vector<std::reference_wrapper<const InputEntry::QuickItem>> GetSelectedQuickItems() const;

	private:
		VirtualListView *									m_list_view;
		std::vector<InputEntry::QuickItem>::const_iterator	m_first, m_last;

		const InputEntry::QuickItem &GetQuickItem(long index) const;
	};
};


//**************************************************************************
//  UTILITY
//**************************************************************************

static axis_type AxisType(const status::input_device_item &item)
{
	axis_type result;
	if (item.m_token == "XAXIS")
		result = axis_type::X;
	else if (item.m_token == "YAXIS")
		result = axis_type::Y;
	else if (item.m_token == "ZAXIS")
		result = axis_type::Z;
	else
		result = axis_type::NONE;
	return result;
}


//**************************************************************************
//  MAIN DIALOG IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::InputsDialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class)
	: wxDialog(&parent, wxID_ANY, title, wxDefaultPosition, wxSize(550, 300), wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
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
	
	// bind the restore button
	Bind(wxEVT_BUTTON, [this](auto &) { OnRestoreButtonPressed(); }, restore_button.GetId());

	// build list of input sequences
	auto entry_descs = BuildInitialEntryDescriptions(input_class);

	// perform the aggregation process
	for (auto iter = entry_descs.begin(); iter != entry_descs.end(); iter++)
	{
		// only entries with aggregate names are candidates for aggregation
		if (!iter->m_aggregate_name.empty())
		{
			// find the next entry that could potentially be aggregated
			auto iter2 = std::find_if(std::next(iter), entry_descs.end(), [iter](const InputEntryDesc &x)
			{
				return iter->m_aggregate_name == x.m_aggregate_name;
			});
			if (iter2 != entry_descs.end())
			{
				// we've found an entry that could be aggregate it; collapse it if possible
				if (!iter->m_analog_x && iter->m_analog_y && iter2->m_analog_x && !iter2->m_analog_y)
				{
					// the current port has analog X and the one further down the line has analog Y; collapse them
					iter->m_analog_x = iter2->m_analog_x;
					entry_descs.erase(iter2);
				}
				else if (iter->m_analog_x && !iter->m_analog_y && !iter2->m_analog_x && iter2->m_analog_y)
				{
					// the current port has analog Y and the one further down the line has analog X; collapse them
					iter->m_analog_y = iter2->m_analog_y;
					entry_descs.erase(iter2);
				}
			}
		}
	}

	// main grid
	std::unique_ptr<wxFlexGridSizer> grid_sizer = std::make_unique<wxFlexGridSizer>(3);

	// build controls
	m_entries.reserve(entry_descs.size());
	for (const InputEntryDesc &entry_desc : entry_descs)
	{
		const wxString &name = entry_desc.m_analog_x && entry_desc.m_analog_y
			? entry_desc.m_aggregate_name
			: entry_desc.GetSingleInput().m_name;

		// create the controls
		wxButton &main_button		= AddControl<wxButton>(scrolled, *grid_sizer, 0, wxALL | wxEXPAND, id++, name);
		wxButton &menu_button		= AddControl<wxButton>(scrolled, *grid_sizer, 0, wxALL | wxALIGN_CENTRE, id++, wxT("\u25BC"), wxDefaultPosition, wxSize(16, 16));
		wxStaticText &static_text	= AddControl<wxStaticText>(scrolled, *grid_sizer, 1, wxALL | wxALIGN_CENTRE_VERTICAL, id++, util::g_empty_string);

		// create the entry
		std::unique_ptr<InputEntry> entry_ptr;
		if (entry_desc.m_digital)
		{
			// a digital line
			assert(!entry_desc.m_analog_x && !entry_desc.m_analog_y);
			entry_ptr = std::make_unique<SingularInputEntry>(*this, *this, main_button, menu_button, static_text, InputFieldRef(*entry_desc.m_digital), status::input_seq::type::STANDARD);
		}
		else
		{
			// an analog line
			entry_ptr = std::make_unique<MultiAxisInputEntry>(*this, *this, main_button, menu_button, static_text, entry_desc.m_analog_x, entry_desc.m_analog_y);
		}
		InputEntry &entry = *entry_ptr;
		m_entries.push_back(std::move(entry_ptr));

		// update text
		entry.UpdateText();
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
//  FindInputSeq
//-------------------------------------------------

const status::input_seq &InputsDialog::FindInputSeq(const InputFieldRef &field_ref, status::input_seq::type seq_type)
{
	// look up the input port by tag and mask
	const status::input *input = util::find_if_ptr(m_host.GetInputs().get(), [&field_ref](const status::input &x)
	{
		return x.m_port_tag == field_ref.m_port_tag && x.m_mask == field_ref.m_mask;
	});
	assert(input);

	// and once we have that, look up the seq
	return *std::find_if(input->m_seqs.begin(), input->m_seqs.end(), [seq_type](const status::input_seq &x)
	{
		return x.m_type == seq_type;
	});
}


//-------------------------------------------------
//  StartInputPoll
//-------------------------------------------------

void InputsDialog::StartInputPoll(const wxString &label, const InputFieldRef &field_ref, status::input_seq::type seq_type, const wxString &start_seq)
{
	// create title and instruction text
	const char *title_text_format		= start_seq.empty() ? "Specify %s" : "Add To %s";
	const char *instruction_text_format = start_seq.empty() ? "Press key or button to specify %s" : "Press key or button to add to %s";
	wxString title_text			= wxString::Format(title_text_format, label);
	wxString instruction_text	= wxString::Format(instruction_text_format, label);

	// start polling
	m_host.StartPolling(field_ref.m_port_tag, field_ref.m_mask, seq_type, start_seq);

	// present the dialog
	SeqPollingDialog dialog(*this, title_text, instruction_text);
	m_current_dialog = &dialog;
	dialog.ShowModal();
	m_current_dialog = nullptr;

	// stop polling (though this might have happened implicitly)
	m_host.StopPolling();

	// did the user select an input through the dialog?  if so, we need to
	// specify it (as opposed to MAME handling things)
	if (!dialog.DialogSelectedResult().empty())
	{
		// assemble the tokens
		wxString new_tokens = !start_seq.empty()
			? start_seq + wxT(" or ") + dialog.DialogSelectedResult()
			: std::move(dialog.DialogSelectedResult());

		// and set it
		SetInputSeqs({ { field_ref.m_port_tag, field_ref.m_mask, seq_type, std::move(new_tokens) } });
	}
}


//-------------------------------------------------
//  OnInputsChanged
//-------------------------------------------------

void InputsDialog::OnInputsChanged()
{
	for (const std::unique_ptr<InputEntry> &entry : m_entries)
		entry->UpdateText();
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
//  OnRestoreButtonPressed
//-------------------------------------------------

void InputsDialog::OnRestoreButtonPressed()
{
	std::vector<SetInputSeqRequest> seqs;
	seqs.reserve(m_entries.size());

	for (const std::unique_ptr<InputEntry> &entry : m_entries)
	{
		for (auto &[field_ref, seq_type] : entry->GetInputSeqRefs())
			seqs.emplace_back(std::move(field_ref.m_port_tag), field_ref.m_mask, seq_type, wxT("*"));
	}	

	SetInputSeqs(std::move(seqs));
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
//  CompareInputs
//-------------------------------------------------

bool InputsDialog::CompareInputs(const status::input &a, const status::input &b)
{
	// logic follows src/frontend/mame/ui/inputmap.cpp in core MAME
	if (a.m_group < b.m_group)
		return true;
	else if (a.m_group > b.m_group)
		return false;
	else if (a.m_type < b.m_type)
		return true;
	else if (a.m_type > b.m_type)
		return false;
	else if (a.m_first_keyboard_code < b.m_first_keyboard_code)
		return true;
	else if (a.m_first_keyboard_code > b.m_first_keyboard_code)
		return false;
	else
		return a.m_name < b.m_name;
}


//-------------------------------------------------
//  BuildInitialEntryDescriptions
//-------------------------------------------------

std::vector<InputEntryDesc> InputsDialog::BuildInitialEntryDescriptions(status::input::input_class input_class) const
{
	std::vector<InputEntryDesc> results;
	for (const status::input &input : m_host.GetInputs().get())
	{
		// is this input of the right class?
		if (input.m_class == input_class)
		{
			// because of how the LUA "fields" property works, there may be dupes; only add if this
			// is not a dupe
			if (!util::find_if_ptr(results, [&input](const InputEntryDesc &x) { return x.GetSingleInput() == input; }))
			{
				wxString::const_reverse_iterator iter;

				InputEntryDesc &entry = results.emplace_back();
				if (input.m_is_analog)
				{
					// is this an analog input that we feel should be presented as being an "X axis" or
					// a "Y axis" in our UI?
					iter = std::find_if(input.m_name.rbegin(), input.m_name.rend(), [](wxChar ch)
					{
						return ch != ' ' && (ch < '0' || ch > '9');
					});
					if (iter < input.m_name.rend() && *iter == 'Y')
						entry.m_analog_y = &input;
					else
						entry.m_analog_x = &input;

					// if this a name that indicates that this name is worthy of aggregation (e.g. - "FooStick [X|Y|Z")?  if
					// so aggregate it
					if (iter < input.m_name.rend() && ((*iter == 'X') || (*iter == 'Y') || (*iter == 'Z')))
					{
						iter++;
						if (iter < input.m_name.rend() && *iter == ' ')
							iter++;
						entry.m_aggregate_name = input.m_name.substr(0, input.m_name.size() - (iter - input.m_name.rbegin()));
					}
				}
				else
				{
					// digital inputs are simple
					entry.m_digital = &input;
				}
			}
		}
	}

	// shrink and sort the results
	results.shrink_to_fit();
	std::sort(results.begin(), results.end(), [](const InputEntryDesc &a, const InputEntryDesc &b)
	{
		return CompareInputs(a.GetSingleInput(), b.GetSingleInput());
	});
	return results;
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
	for (wxString &token : tokens)
	{
		wxString word;
		if (token == "OR" || token == "NOT" || token == "DEFAULT")
		{
			// modifier tokens; just "lowercase" it (this will need to be reevaluated when it 
			// is time to localize this
			word = token.Lower();
		}
		else
		{
			// look up this token, but we need to remove modifiers
			auto [token_base, modifier] = ParseIndividualToken(std::move(token));

			// now do the lookup
			auto iter = m_codes.find(token_base);
			if (iter != m_codes.end())
				word = iter->second;

			// do we have a modifier?  if so, append it
			if (!modifier.empty())
			{
				// another thing that will need to be reevaluated when we want to localize
				modifier = modifier.substr(0, 1) + modifier.substr(1).Lower();

				// append the "localized" modifier
				word += wxT(" ");
				word += modifier;
			}
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

	return result;
}


//-------------------------------------------------
//  ParseIndividualToken
//-------------------------------------------------

std::tuple<wxString, wxString> InputsDialog::ParseIndividualToken(wxString &&token)
{
	// token parsing
	wxString::iterator current_position = token.begin();
	auto next_token = [&current_position, &token]() -> wxString::iterator
	{
		wxString::iterator iter = std::find(current_position, token.end(), '_');
		current_position = iter;
		while (current_position < token.end() && *current_position == '_')
			current_position++;
		return iter;
	};

	// parse out the item part
	wxString::iterator item_end = next_token();
	if (current_position < token.end() && *current_position >= '0' && *current_position <= '9')
		item_end = next_token();	// device number
	item_end = next_token();

	// do we have more?  if so, parse out the modifier
	wxString modifier;
	if (item_end < token.end())
	{
		wxString::iterator modifier_begin = current_position;
		wxString::iterator modifier_end = next_token();
		modifier = wxString(modifier_begin, modifier_end);
		token.resize(item_end - token.begin());
	}

	return { std::move(token), std::move(modifier) };
}


//**************************************************************************
//  SEQ POLLING DIALOG
//**************************************************************************

//-------------------------------------------------
//  SeqPollingDialog ctor
//-------------------------------------------------

SeqPollingDialog::SeqPollingDialog(InputsDialog &host, const wxString &title_text, const wxString &instructions_text)
	: wxDialog(&host, wxID_ANY, title_text, wxDefaultPosition, wxSize(350, 200))
	, m_host(host)
	, m_mouse_inputs_button(nullptr)
{
	// controls
	int id = wxID_LAST + 1;
	wxStaticText &instructions_static_text	= *new wxStaticText(this, wxID_ANY, instructions_text);
	wxStaticText &spacer_static_text		= *new wxStaticText(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(50, 20));
	wxStaticText &mouse_inputs_static_text	= *new wxStaticText(this, wxID_ANY, wxT("For mouse inputs, press the button to the right"));
	m_mouse_inputs_button					= new wxButton(this, id++, wxT("Mouse Inputs"));
	wxButton &decoy_button					= *new wxButton(this, id++, wxT(""), wxPoint(-1000, -1000));

	// events
	Bind(wxEVT_BUTTON, [this](auto &) { OnMouseButtonPressed(); }, m_mouse_inputs_button->GetId());

	// we don't want the mouse inputs button to have focus (it might be triggered by the user specifying inputs), so
	// we're creating a decoy button and giving it focus
	decoy_button.SetFocus();

	// sizer layout
	SpecifySizerAndFit(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 1, wxALL,					instructions_static_text },
		{ 1, wxALL | wxEXPAND,		spacer_static_text },
		{ 0, wxALL | wxALIGN_RIGHT,	boxsizer_orientation::HORIZONTAL, 4, {
			{ 0, wxALL | wxALIGN_CENTER_VERTICAL, mouse_inputs_static_text },
			{ 0, wxALL | wxALIGN_CENTER_VERTICAL, *m_mouse_inputs_button }
		}}
	} });
}


//-------------------------------------------------
//  SeqPollingDialog::OnMouseButtonPressed
//-------------------------------------------------

void SeqPollingDialog::OnMouseButtonPressed()
{
	enum
	{
		ID_START = wxID_HIGHEST + 1
	};

	// identify pertinent mouse inputs
	std::vector<const status::input_device_item *> items;
	for (const status::input_class &devclass : m_host.GetInputClasses())
	{
		if (devclass.m_name == "mouse")
		{
			for (const status::input_device &dev : devclass.m_devices)
			{
				for (const status::input_device_item &item : dev.m_items)
				{
					if (AxisType(item) == axis_type::NONE)
						items.emplace_back(&item);
				}
			}
		}
	}

	// sort the mouse inputs
	std::sort(items.begin(), items.end(), [](const status::input_device_item *x, const status::input_device_item *y)
	{
		return x->m_code < y->m_code;
	});

	// build the popup menu
	MenuWithResult popup_menu;
	for (int i = 0; i < items.size(); i++)
		popup_menu.Append(ID_START + i, items[i]->m_name);

	// and display it
	wxRect button_rectangle = m_mouse_inputs_button->GetRect();
	if (!PopupMenu(&popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom()))
		return;

	// the user selected a result; specify the result and close out
	if (popup_menu.Result() >= ID_START)
		m_dialog_selected_result = items[popup_menu.Result() - ID_START]->m_code;
	Close();
}



//**************************************************************************
//  INPUT ENTRY DESCRIPTIONS
//**************************************************************************

//-------------------------------------------------
//  InputEntryDesc ctor
//-------------------------------------------------

InputEntryDesc::InputEntryDesc()
	: m_digital(nullptr)
	, m_analog_x(nullptr)
	, m_analog_y(nullptr)
{
}


//-------------------------------------------------
//  InputEntryDesc::GetSingleInput
//-------------------------------------------------

const status::input &InputEntryDesc::GetSingleInput() const
{
	// this expects only a single input to be specified
	assert((m_digital && !m_analog_x && !m_analog_y)
		|| (!m_digital && m_analog_x && !m_analog_y)
		|| (!m_digital && !m_analog_x && m_analog_y));

	// return the appropriate one
	if (m_analog_x)
		return *m_analog_x;
	else if (m_analog_y)
		return *m_analog_y;
	else
		return *m_digital;
}



//**************************************************************************
//  INPUT ENTRIES
//**************************************************************************

//-------------------------------------------------
//  InputEntry ctor
//-------------------------------------------------

InputEntry::InputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text)
	: m_parent(parent)
	, m_host(host)
	, m_main_button(main_button)
	, m_menu_button(menu_button)
	, m_static_text(static_text)
{
	m_parent.Bind(wxEVT_BUTTON, [this](auto &) { OnMainButtonPressed(); }, m_main_button.GetId());
	m_parent.Bind(wxEVT_BUTTON, [this](auto &) { OnMenuButtonPressed(); }, m_menu_button.GetId());
}


//-------------------------------------------------
//  InputEntry::UpdateText
//-------------------------------------------------

void InputEntry::UpdateText()
{
	// get the text (which behaves differently for digital and analog)
	wxString text = GetText();

	// show something at least
	if (text.empty())
		text = "None";

	// and set the label
	m_static_text.SetLabel(text);
}


//-------------------------------------------------
//  InputEntry::PopupMenu
//-------------------------------------------------

bool InputEntry::PopupMenu(wxMenu &popup_menu)
{
	wxRect button_rectangle = m_menu_button.GetRect();
	return m_parent.PopupMenu(&popup_menu, button_rectangle.GetLeft(), button_rectangle.GetBottom());
}


//-------------------------------------------------
//  BuildQuickItems
//-------------------------------------------------

std::vector<InputEntry::QuickItem> InputEntry::BuildQuickItems(const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref, const std::optional<InputFieldRef> &all_axes_field_ref)
{
	std::vector<QuickItem> results;

	// precanned arrow/number keys
	if (x_field_ref || y_field_ref)
	{
		results.resize(3);
		QuickItem &clear_quick_item = results[0];
		QuickItem &arrows_quick_item = results[1];
		QuickItem &numpad_quick_item = results[2];
		clear_quick_item.m_label = "Clear";
		arrows_quick_item.m_label = "Arrow Keys";
		numpad_quick_item.m_label = "Numeric Keypad";

		if (x_field_ref)
		{
			clear_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			clear_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::DECREMENT, wxString());
			clear_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::INCREMENT, wxString());

			arrows_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			arrows_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::DECREMENT, wxT("KEYCODE_LEFT"));
			arrows_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::INCREMENT, wxT("KEYCODE_RIGHT"));

			numpad_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			numpad_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::DECREMENT, wxT("KEYCODE_4PAD"));
			numpad_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::INCREMENT, wxT("KEYCODE_6PAD"));
		}

		if (y_field_ref)
		{
			clear_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			clear_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::DECREMENT, wxString());
			clear_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::INCREMENT, wxString());

			arrows_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			arrows_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::DECREMENT, wxT("KEYCODE_UP"));
			arrows_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::INCREMENT, wxT("KEYCODE_DOWN"));

			numpad_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::STANDARD, wxString());
			numpad_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::DECREMENT, wxT("KEYCODE_8PAD"));
			numpad_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::INCREMENT, wxT("KEYCODE_2PAD"));
		}
	}

	// build the results based on analog devices from MAME
	const std::vector<status::input_class> &devclasses = Host().GetInputClasses();
	for (const status::input_class &devclass : devclasses)
	{
		for (const status::input_device &dev : devclass.m_devices)
		{
			QuickItem dev_quick_item;

			for (const status::input_device_item &item : dev.m_items)
			{
				axis_type at = AxisType(item);
				if (x_field_ref && at == axis_type::X)
				{
					dev_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::STANDARD, item.m_code);
					dev_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::DECREMENT, wxString());
					dev_quick_item.m_selections.emplace_back(x_field_ref->m_port_tag, x_field_ref->m_mask, status::input_seq::type::INCREMENT, wxString());
				}
				if (y_field_ref && at == axis_type::Y)
				{
					dev_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::STANDARD, item.m_code);
					dev_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::DECREMENT, wxString());
					dev_quick_item.m_selections.emplace_back(y_field_ref->m_port_tag, y_field_ref->m_mask, status::input_seq::type::INCREMENT, wxString());
				}
				if (all_axes_field_ref && at != axis_type::NONE)
				{
					QuickItem &axis_quick_item = results.emplace_back();
					axis_quick_item.m_label = wxString::Format("%s #%d %s (%s)",
						InputsDialog::GetDeviceClassName(devclass, false),
						dev.m_index + 1,
						item.m_name,
						dev.m_name);
					axis_quick_item.m_selections.emplace_back(all_axes_field_ref->m_port_tag, all_axes_field_ref->m_mask, status::input_seq::type::STANDARD, item.m_code);
					axis_quick_item.m_selections.emplace_back(all_axes_field_ref->m_port_tag, all_axes_field_ref->m_mask, status::input_seq::type::DECREMENT, wxString());
					axis_quick_item.m_selections.emplace_back(all_axes_field_ref->m_port_tag, all_axes_field_ref->m_mask, status::input_seq::type::INCREMENT, wxString());
				}
			}

			if (!dev_quick_item.m_selections.empty())
			{
				dev_quick_item.m_label = wxString::Format("%s #%d (%s)",
					InputsDialog::GetDeviceClassName(devclass, false),
					dev.m_index + 1,
					dev.m_name);
				results.push_back(std::move(dev_quick_item));
			}
		}
	}
	return results;
}


//-------------------------------------------------
//  InputEntry::InvokeQuickItem
//-------------------------------------------------

bool InputEntry::InvokeQuickItem(std::vector<InputEntry::QuickItem> &&quick_items, int index)
{
	if (index < 0 || index >= quick_items.size())
		return false;

	Host().SetInputSeqs(std::move(quick_items[index].m_selections));
	return true;
}


//-------------------------------------------------
//  InputEntry::ShowMultipleQuickItemsDialog
//-------------------------------------------------

bool InputEntry::ShowMultipleQuickItemsDialog(std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last)
{
	MultipleQuickItemsDialog dialog(m_parent, first, last);
	if (dialog.ShowModal() != wxID_OK)
		return false;

	// merge the quick items
	std::vector<SetInputSeqRequest> merged_quick_items;
	for (const QuickItem &item : dialog.GetSelectedQuickItems())
	{
		for (const SetInputSeqRequest &req : item.m_selections)
		{
			auto iter = std::find_if(merged_quick_items.begin(), merged_quick_items.end(), [&req](const SetInputSeqRequest &x)
			{
				return x.m_port_tag == req.m_port_tag
					&& x.m_mask == req.m_mask
					&& x.m_seq_type == req.m_seq_type;
			});
			if (iter == merged_quick_items.end())
			{
				SetInputSeqRequest &new_merged_quick_item = merged_quick_items.emplace_back();
				new_merged_quick_item.m_port_tag = req.m_port_tag;
				new_merged_quick_item.m_mask = req.m_mask;
				new_merged_quick_item.m_seq_type = req.m_seq_type;
				iter = merged_quick_items.end() - 1;
			}
			if (!iter->m_tokens.empty())
				iter->m_tokens += wxT(" or ");
			iter->m_tokens += req.m_tokens;
		}
	}

	// and specify them
	Host().SetInputSeqs(std::move(merged_quick_items));
	return true;
}


//-------------------------------------------------
//  SingularInputEntry ctor
//-------------------------------------------------

SingularInputEntry::SingularInputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text, InputFieldRef &&field_ref, status::input_seq::type seq_type)
	: InputEntry(parent, host, main_button, menu_button, static_text)
	, m_field_ref(std::move(field_ref))
	, m_seq_type(seq_type)
{
}


//-------------------------------------------------
//  SingularInputEntry::GetText
//-------------------------------------------------

wxString SingularInputEntry::GetText()
{
	std::vector<wxString> parts;

	// if this is not for input_seq::type::STANDARD(and hence, this is a part of the MultiAxisInputDialog), then
	// we have to display the standard seq
	if (m_seq_type != status::input_seq::type::STANDARD)
	{
		const status::input_seq &standard_seq = Host().FindInputSeq(m_field_ref, status::input_seq::type::STANDARD);
		wxString standard_text = Host().GetSeqTextFromTokens(standard_seq.m_tokens);
		if (!standard_text.empty())
			parts.push_back(std::move(standard_text));
	}

	const status::input_seq &seq = Host().FindInputSeq(m_field_ref, m_seq_type);
	wxString text = Host().GetSeqTextFromTokens(seq.m_tokens);
	if (!text.empty())
		parts.push_back(std::move(text));

	return util::string_join(wxString(wxT(" / ")), parts);
}


//-------------------------------------------------
//  SingularInputEntry::OnMainButtonPressed
//-------------------------------------------------

void SingularInputEntry::OnMainButtonPressed()
{
	Host().StartInputPoll(MainButton().GetLabel(), m_field_ref, m_seq_type);
}


//-------------------------------------------------
//  SingularInputEntry::OnMenuButtonPressed
//-------------------------------------------------

bool SingularInputEntry::OnMenuButtonPressed()
{
	enum
	{
		ID_SPECIFY_INPUT = wxID_HIGHEST + 1,
		ID_ADD_INPUT,
		ID_CLEAR_INPUT,
		ID_MULTIPLE_QUICK_ITEMS,
		ID_QUICK_ITEMS_BEGIN
	};

	// if this is not for input_seq::type::STANDARD (and hence, this is a part of the MultiAxisInputDialog), then
	// we have quick items
	std::vector<QuickItem> quick_items = m_seq_type != status::input_seq::type::STANDARD
		? BuildQuickItems({ }, { }, m_field_ref)
		: std::vector<QuickItem>();
	
	// create the pop up menu
	MenuWithResult popup_menu;

	// append any quick items
	if (!quick_items.empty())
	{
		for (int i = 0; i < quick_items.size(); i++)
			popup_menu.Append(ID_QUICK_ITEMS_BEGIN + i, quick_items[i].m_label);
		popup_menu.Append(ID_MULTIPLE_QUICK_ITEMS, s_menu_item_text_multiple);
		popup_menu.AppendSeparator();
	}

	// append the normal items
	popup_menu.Append(ID_SPECIFY_INPUT, s_menu_item_text_specify);
	popup_menu.Append(ID_ADD_INPUT, s_menu_item_text_add);
	popup_menu.Append(ID_CLEAR_INPUT, s_menu_item_text_clear);

	// show the pop up menu
	if (!PopupMenu(popup_menu))
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
				? &Host().FindInputSeq(m_field_ref, m_seq_type)
				: nullptr;

			// based on that, identify the sequence of tokens that we're starting on
			const wxString &start_seq_tokens = append_to_seq
				? append_to_seq->m_tokens
				: util::g_empty_string;

			// and start polling!
			Host().StartInputPoll(MainButton().GetLabel(), m_field_ref, m_seq_type, start_seq_tokens);
			break;
		}
		break;

	case ID_CLEAR_INPUT:
		{
			std::vector<SetInputSeqRequest> reqs;
			SetInputSeqRequest &req1 = reqs.emplace_back();
			req1.m_port_tag = m_field_ref.m_port_tag;
			req1.m_mask = m_field_ref.m_mask;
			req1.m_seq_type = m_seq_type;

			if (m_seq_type != status::input_seq::type::STANDARD)
			{
				SetInputSeqRequest &req2 = reqs.emplace_back();
				req2.m_port_tag = m_field_ref.m_port_tag;
				req2.m_mask = m_field_ref.m_mask;
				req2.m_seq_type = status::input_seq::type::STANDARD;
			}
			Host().SetInputSeqs(std::move(reqs));
		}
		break;

	case ID_MULTIPLE_QUICK_ITEMS:
		if (!ShowMultipleQuickItemsDialog(std::next(quick_items.begin()), quick_items.end()))
			return false;
		break;

	default:
		// anothing else is a quick item
		if (!InvokeQuickItem(std::move(quick_items), popup_menu.Result() - ID_QUICK_ITEMS_BEGIN))
			return false;
		break;
	}
	return true;
}


//-------------------------------------------------
//  SingularInputEntry::GetInputSeqRefs
//-------------------------------------------------

std::vector<std::tuple<InputFieldRef, status::input_seq::type>> SingularInputEntry::GetInputSeqRefs()
{
	return
	{
		{
			{ m_field_ref.m_port_tag, m_field_ref.m_mask },
			m_seq_type
		}
	};
}


//-------------------------------------------------
//  MultiAxisInputEntry ctor
//-------------------------------------------------

MultiAxisInputEntry::MultiAxisInputEntry(wxWindow &parent, InputsDialog &host, wxButton &main_button, wxButton &menu_button, wxStaticText &static_text, const status::input *x_input, const status::input *y_input)
	: InputEntry(parent, host, main_button, menu_button, static_text)
{
	// sanity checks
	assert(x_input || x_input);

	if (x_input)
		m_x_field_ref.emplace(*x_input);
	if (y_input)
		m_y_field_ref.emplace(*y_input);
}


//-------------------------------------------------
//  MultiAxisInputEntry::GetText
//-------------------------------------------------

wxString MultiAxisInputEntry::GetText()
{
	std::vector<std::tuple<char16_t, std::reference_wrapper<InputFieldRef>, status::input_seq::type>> seqs;
	seqs.reserve(6);
	if (m_x_field_ref)
	{
		seqs.emplace_back(0x2194, *m_x_field_ref, status::input_seq::type::STANDARD);
		seqs.emplace_back(0x25C0, *m_x_field_ref, status::input_seq::type::DECREMENT);
		seqs.emplace_back(0x25B6, *m_x_field_ref, status::input_seq::type::INCREMENT);
	}
	if (m_y_field_ref)
	{
		seqs.emplace_back(0x2195, *m_y_field_ref, status::input_seq::type::STANDARD);
		seqs.emplace_back(0x25B2, *m_y_field_ref, status::input_seq::type::DECREMENT);
		seqs.emplace_back(0x25BC, *m_y_field_ref, status::input_seq::type::INCREMENT);
	}

	wxString result;
	for (const auto &[ch, field_ref, seq_type] : seqs)
	{
		const status::input_seq &seq = Host().FindInputSeq(field_ref, seq_type);
		wxString seq_text = Host().GetSeqTextFromTokens(seq.m_tokens);

		if (!seq_text.empty())
		{
			if (!result.empty())
				result += wxT(" / ");
			result += ch;
			result += seq_text;
		}
	}
	return result;
}


//-------------------------------------------------
//  MultiAxisInputEntry::OnMainButtonPressed
//-------------------------------------------------

void MultiAxisInputEntry::OnMainButtonPressed()
{
	Specify();
}


//-------------------------------------------------
//  MultiAxisInputEntry::OnMenuButtonPressed
//-------------------------------------------------

bool MultiAxisInputEntry::OnMenuButtonPressed()
{
	enum
	{
		ID_SPECIFY_INPUT = wxID_HIGHEST + 1,
		ID_MULTIPLE_QUICK_ITEMS,
		ID_QUICK_ITEMS_BEGIN
	};

	// keep a list of so-called "quick" items; direct specifications of input seqs
	std::vector<QuickItem> quick_items = BuildQuickItems(m_x_field_ref, m_y_field_ref, { });

	// create the pop up menu
	MenuWithResult popup_menu;

	// append quick items to the pop up menu (item #0 is clear; we do that later)
	for (int i = 1; i < quick_items.size(); i++)
		popup_menu.Append(ID_QUICK_ITEMS_BEGIN + i, quick_items[i].m_label);

	// finally append "multiple...", a separator, specify and clear
	popup_menu.Append(ID_MULTIPLE_QUICK_ITEMS, s_menu_item_text_multiple);
	popup_menu.AppendSeparator();
	popup_menu.Append(ID_SPECIFY_INPUT, s_menu_item_text_specify);
	popup_menu.Append(ID_QUICK_ITEMS_BEGIN + 0, quick_items[0].m_label);

	// show the pop up menu
	if (!PopupMenu(popup_menu))
		return false;

	switch(popup_menu.Result())
	{
	case ID_SPECIFY_INPUT:
		Specify();
		break;

	case ID_MULTIPLE_QUICK_ITEMS:
		if (!ShowMultipleQuickItemsDialog(std::next(quick_items.begin()), quick_items.end()))
			return false;
		break;

	default:
		// everything else is a quick item
		if (!InvokeQuickItem(std::move(quick_items), popup_menu.Result() - ID_QUICK_ITEMS_BEGIN))
			return false;
		break;
	}
	return true;
}


//-------------------------------------------------
//  MultiAxisInputEntry::Specify
//-------------------------------------------------

bool MultiAxisInputEntry::Specify()
{
	MultiAxisInputDialog dialog(Host(), MainButton().GetLabel(), m_x_field_ref, m_y_field_ref);
	return dialog.ShowModal() == wxID_OK;
}


//-------------------------------------------------
//  MultiAxisInputEntry::GetInputSeqRefs
//-------------------------------------------------

std::vector<std::tuple<InputFieldRef, status::input_seq::type>> MultiAxisInputEntry::GetInputSeqRefs()
{
	std::vector<std::tuple<InputFieldRef, status::input_seq::type>> results;
	if (m_x_field_ref)
	{
		results.emplace_back(*m_x_field_ref, status::input_seq::type::STANDARD);
		results.emplace_back(*m_x_field_ref, status::input_seq::type::DECREMENT);
		results.emplace_back(*m_x_field_ref, status::input_seq::type::INCREMENT);
	}
	if (m_y_field_ref)
	{
		results.emplace_back(*m_y_field_ref, status::input_seq::type::STANDARD);
		results.emplace_back(*m_y_field_ref, status::input_seq::type::DECREMENT);
		results.emplace_back(*m_y_field_ref, status::input_seq::type::INCREMENT);
	}
	return results;
}


//**************************************************************************
//  MULTI AXIS INPUT DIALOG
//**************************************************************************

//-------------------------------------------------
//  MultiAxisInputDialog ctor
//-------------------------------------------------

MultiAxisInputDialog::MultiAxisInputDialog(InputsDialog &parent, const wxString &title, const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref)
	: wxDialog(&parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_host(parent)
{
	int id = wxID_LAST + 1;
	m_singular_inputs.reserve(4);

	// come up with names for each of the axes
	wxString x_axis_label = x_field_ref && y_field_ref ? title + " X" : title;
	wxString y_axis_label = x_field_ref && y_field_ref ? title + " Y" : title;

	// use a grid sizer for the main controls
	auto grid_sizer = std::make_unique<wxGridSizer>(3, 3, 4, 4);
	grid_sizer->Add(&AddSpacer());
	grid_sizer->Add(&AddInputPanel(y_field_ref, status::input_seq::type::DECREMENT, id, y_axis_label + " Dec"), 0, wxALIGN_TOP);
	grid_sizer->Add(&AddSpacer());
	grid_sizer->Add(&AddInputPanel(x_field_ref, status::input_seq::type::DECREMENT, id, x_axis_label + " Dec"), 0, wxALIGN_LEFT);
	grid_sizer->Add(&AddSpacer());
	grid_sizer->Add(&AddInputPanel(x_field_ref, status::input_seq::type::INCREMENT, id, x_axis_label + " Inc"), 0, wxALIGN_RIGHT);
	grid_sizer->Add(&AddSpacer());
	grid_sizer->Add(&AddInputPanel(y_field_ref, status::input_seq::type::INCREMENT, id, y_axis_label + " Inc"), 0, wxALIGN_BOTTOM);
	grid_sizer->Add(&AddSpacer());

	// update the text
	for (SingularInputEntry &entry : m_singular_inputs)
		entry.UpdateText();

	// subscribe to events
	m_inputs_subscription = Host().GetInputs().subscribe([this]() { OnInputsChanged(); });

	// clear and restore buttons
	wxButton &clear_button		= *new wxButton(this, id++, wxT("Clear"));
	wxButton &restore_button	= *new wxButton(this, id++, wxT("Restore Defaults"));
	Bind(wxEVT_BUTTON, [this](auto &) { SetAllInputs(wxT(""));	}, clear_button.GetId());
	Bind(wxEVT_BUTTON, [this](auto &) { SetAllInputs(wxT("*"));	}, restore_button.GetId());

	// specify the sizer
	SpecifySizerAndFit(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 1, wxALL | wxEXPAND,		grid_sizer.release() },
		{ 0, wxALL | wxALIGN_RIGHT,	boxsizer_orientation::HORIZONTAL, 4, {
			{ 0, wxALL, clear_button },
			{ 0, wxALL, restore_button },
			{ 0, wxALL, CreateButtonSizer(wxOK) }
		}}
	}});
}


//-------------------------------------------------
//  MultiAxisInputDialog::AddInputPanel
//-------------------------------------------------

wxSizer &MultiAxisInputDialog::AddInputPanel(const std::optional<InputFieldRef> &field_ref, status::input_seq::type seq_type, int &id, wxString &&label)
{
	// create the controls
	wxButton &main_button		= *new wxButton(this, id++, field_ref ? label : "n/a");
	wxButton &menu_button		= *new wxButton(this, id++, wxT("\u25BC"), wxDefaultPosition, wxSize(16, 16));
	wxStaticText &static_text	= *new wxStaticText(this, wxID_ANY, "n/a");

	// only enable these controls if something is specified
	main_button.Enable(field_ref.has_value());
	menu_button.Enable(field_ref.has_value());
	static_text.Enable(field_ref.has_value());

	// some activities that only apply when we have a real field
	if (field_ref)
	{
		// keep track of this input
		m_singular_inputs.emplace_back(*this, Host(), main_button, menu_button, static_text, InputFieldRef(*field_ref), seq_type);
	}
	
	// and do the sizer work
	std::unique_ptr<wxBoxSizer> h_sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
	h_sizer->Add(&main_button, 0, wxLEFT | wxRIGHT | wxEXPAND, 4);
	h_sizer->Add(&menu_button, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE, 4);
	std::unique_ptr<wxBoxSizer> v_sizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
	v_sizer->Add(&static_text, 0, wxALIGN_CENTER_HORIZONTAL, 0);
	v_sizer->Add(h_sizer.release(), 0, wxEXPAND, 0);
	return *v_sizer.release();
}


//-------------------------------------------------
//  MultiAxisInputDialog::AddSpacer
//-------------------------------------------------

wxWindow &MultiAxisInputDialog::AddSpacer()
{
	// this is silly
	return *new wxStaticText(this, wxID_ANY, wxT(""));
}


//-------------------------------------------------
//  MultiAxisInputDialog::OnInputsChanged
//-------------------------------------------------

void MultiAxisInputDialog::OnInputsChanged()
{
	for (SingularInputEntry &entry : m_singular_inputs)
		entry.UpdateText();
}


//-------------------------------------------------
//  MultiAxisInputDialog::SetAllInputs
//-------------------------------------------------

void MultiAxisInputDialog::SetAllInputs(const wxString &tokens)
{
	std::vector<SetInputSeqRequest> seqs;
	seqs.reserve(m_singular_inputs.size());

	for (SingularInputEntry &entry : m_singular_inputs)
	{
		for (auto &[field_ref, seq_type] : entry.GetInputSeqRefs())
			seqs.emplace_back(std::move(field_ref.m_port_tag), field_ref.m_mask, seq_type, tokens);
	}

	Host().SetInputSeqs(std::move(seqs));
}


//**************************************************************************
//  MULTIPLE QUICK ITEMS DIALOG
//**************************************************************************

//-------------------------------------------------
//  MultipleQuickItemsDialog ctor
//-------------------------------------------------

MultipleQuickItemsDialog::MultipleQuickItemsDialog(wxWindow &parent, std::vector<InputEntry::QuickItem>::const_iterator first, std::vector<InputEntry::QuickItem>::const_iterator last)
	: wxDialog(&parent, wxID_ANY, wxT("Multiple"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX | wxRESIZE_BORDER)
	, m_list_view(nullptr)
	, m_first(first)
	, m_last(last)
{
	int id = wxID_LAST + 1;

	// create the list view
	m_list_view = new VirtualListView(this, id++);
	m_list_view->AppendColumn(wxT("Item"));
	m_list_view->SetOnGetItemText([this](long item, long)
	{
		return GetQuickItem(item).m_label;
	});
	m_list_view->SetItemCount(last - first);

	// specify the sizer
	SpecifySizer(*this, { boxsizer_orientation::VERTICAL, 10, {
		{ 1, wxALL | wxEXPAND,		*m_list_view },
		{ 0, wxALL | wxALIGN_RIGHT,	CreateButtonSizer(wxOK | wxCANCEL) }
	} });

	// set the column width
	m_list_view->SetColumnWidth(0, m_list_view->GetSize().GetWidth());
}


//-------------------------------------------------
//  MultipleQuickItemsDialog::GetSelectedQuickItems
//-------------------------------------------------

std::vector<std::reference_wrapper<const InputEntry::QuickItem>> MultipleQuickItemsDialog::GetSelectedQuickItems() const
{
	std::vector<std::reference_wrapper<const InputEntry::QuickItem>> results;
	results.reserve(m_list_view->GetSelectedItemCount());

	long item = -1;
	while ((item = m_list_view->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) >= 0)
	{
		const InputEntry::QuickItem &qi = GetQuickItem(item);
		results.push_back(qi);
	}

	return results;
}


//-------------------------------------------------
//  MultipleQuickItemsDialog::GetQuickItem
//-------------------------------------------------

const InputEntry::QuickItem &MultipleQuickItemsDialog::GetQuickItem(long index) const
{
	auto iter = m_first + index;
	assert(iter < m_last);
	return *iter;
}


//**************************************************************************
//  ENTRY POINT
//**************************************************************************

//-------------------------------------------------
//  show_inputs_dialog
//-------------------------------------------------

bool show_inputs_dialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class)
{
	InputsDialog dialog(parent, title, host, input_class);
	return dialog.ShowModal() == wxID_OK;
}
