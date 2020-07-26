/***************************************************************************

    dialogs/inputs.cpp

    Input customization dialog

***************************************************************************/

#include <QPushButton>
#include <QLabel>

#include "dialogs/inputs.h"
#include "dialogs/inputs_seqpoll.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> InputEntryDesc
// represents an entry as a part of the input analysis process
struct InputsDialog::InputEntryDesc
{
	// ctor
	InputEntryDesc()
		: m_digital(nullptr)
		, m_analog_x(nullptr)
		, m_analog_y(nullptr)
	{
	}

	// fields
	const status::input *m_digital;
	const status::input *m_analog_x;
	const status::input *m_analog_y;
	QString				m_aggregate_name;

	// methods
	const status::input &GetSingleInput() const
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
};


// ======================> InputFieldRef
struct InputsDialog::InputFieldRef
{
public:
	QString				m_port_tag;
	ioport_value		m_mask;

	InputFieldRef()
		: m_mask(0)
	{
	}

	InputFieldRef(const QString &port_tag, ioport_value mask)
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
class InputsDialog::InputEntry
{
public:
	struct QuickItem
	{
		QString							m_label;
		std::vector<SetInputSeqRequest>	m_selections;
	};

	InputEntry(InputsDialog &host, QPushButton &main_button, QPushButton &menu_button, QLabel &static_text)
		: m_host(host)
		, m_main_button(main_button)
		, m_menu_button(menu_button)
		, m_static_text(static_text)
	{
		m_host.connect(&m_main_button, &QPushButton::clicked, &m_host, [this]() { OnMainButtonPressed(); });
		m_host.connect(&m_menu_button, &QPushButton::clicked, &m_host, [this]() { OnMenuButtonPressed(); });
	}

	virtual ~InputEntry()
	{
	}

	void UpdateText()
	{
		// get the text (which behaves differently for digital and analog)
		QString text = GetText();

		// show something at least
		if (text.isEmpty())
			text = "None";

		// and set the label
		m_static_text.setText(text);
	}

	virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() = 0;

protected:
	// overridden by child classes
	virtual QString GetText() = 0;
	virtual void OnMainButtonPressed() = 0;
	virtual bool OnMenuButtonPressed() = 0;

	// accessors
	InputsDialog &Host()		{ return m_host; }
	QPushButton &MainButton()	{ return m_main_button; }

	// methods
#if 0
	bool PopupMenu(wxMenu &popup_menu);
	std::vector<QuickItem> BuildQuickItems(const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref, const std::optional<InputFieldRef> &all_axes_field_ref);
	bool InvokeQuickItem(std::vector<InputEntry::QuickItem> &&quick_items, int index);
	bool ShowMultipleQuickItemsDialog(std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last);
#endif

private:
	InputsDialog &m_host;
	QPushButton &m_main_button;
	QPushButton &m_menu_button;
	QLabel &m_static_text;
};


// ======================> SingularInputEntry
class InputsDialog::SingularInputEntry : public InputEntry
{
public:
	SingularInputEntry(InputsDialog &host, QPushButton &main_button, QPushButton &menu_button, QLabel &static_text, InputFieldRef &&field_ref, status::input_seq::type seq_type)
		: InputEntry(host, main_button, menu_button, static_text)
		, m_field_ref(std::move(field_ref))
		, m_seq_type(seq_type)
	{
	}

	virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() override
	{
		return
		{
			{
				{ m_field_ref.m_port_tag, m_field_ref.m_mask },
				m_seq_type
			}
		};
	}

protected:
	virtual QString GetText() override
	{
		QStringList parts;

		// if this is not for input_seq::type::STANDARD(and hence, this is a part of the MultiAxisInputDialog), then
		// we have to display the standard seq
		if (m_seq_type != status::input_seq::type::STANDARD)
		{
			const status::input_seq &standard_seq = Host().FindInputSeq(m_field_ref, status::input_seq::type::STANDARD);
			QString standard_text = Host().GetSeqTextFromTokens(standard_seq.m_tokens);
			if (!standard_text.isEmpty())
				parts.push_back(std::move(standard_text));
		}

		const status::input_seq &seq = Host().FindInputSeq(m_field_ref, m_seq_type);
		QString text = Host().GetSeqTextFromTokens(seq.m_tokens);
		if (!text.isEmpty())
			parts.push_back(std::move(text));

		return parts.join(" / ");
	}

	virtual void OnMainButtonPressed() override
	{
		Host().StartInputPoll(MainButton().text(), m_field_ref, m_seq_type);
	}

	virtual bool OnMenuButtonPressed() override
	{
		throw false;	// NYI
	}

private:
	InputFieldRef			m_field_ref;
	status::input_seq::type	m_seq_type;
};


// ======================> MultiAxisInputEntry
class InputsDialog::MultiAxisInputEntry : public InputEntry
{
public:
	MultiAxisInputEntry(InputsDialog &host, QPushButton &main_button, QPushButton &menu_button, QLabel &static_text, const status::input *x_input, const status::input *y_input)
		: InputEntry(host, main_button, menu_button, static_text)
	{
		throw false;	// NYI
	}

	virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() override
	{
		throw false;	// NYI
	}

protected:
	virtual void OnMainButtonPressed() override
	{
		throw false;	// NYI
	}

	virtual bool OnMenuButtonPressed() override
	{
		throw false;	// NYI
	}

	virtual QString GetText() override
	{
		throw false;	// NYI
	}

private:
	std::optional<InputFieldRef>	m_x_field_ref;
	std::optional<InputFieldRef>	m_y_field_ref;

	bool Specify();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

InputsDialog::InputsDialog(QWidget *parent, IInputsHost &host, status::input::input_class input_class)
	: InputsDialogBase(parent, input_class)
	, m_host(host)
	, m_current_dialog(nullptr)
{
	// build codes map
	m_codes = BuildCodes(m_host.GetInputClasses());

	// build list of input sequences
	auto entry_descs = BuildInitialEntryDescriptions(input_class);

	// perform the aggregation process
	for (auto iter = entry_descs.begin(); iter != entry_descs.end(); iter++)
	{
		// only entries with aggregate names are candidates for aggregation
		if (!iter->m_aggregate_name.isEmpty())
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

	// build controls
	m_entries.reserve(entry_descs.size());
	int row = 0;
	for (const InputEntryDesc &entry_desc : entry_descs)
	{
		const QString &name = entry_desc.m_analog_x && entry_desc.m_analog_y
			? entry_desc.m_aggregate_name
			: entry_desc.GetSingleInput().m_name;

		// create the controls
		QPushButton &main_button = *new QPushButton(name, this);
		main_button.setSizePolicy(QSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum));
		QPushButton &menu_button = *new QPushButton(QString::fromUtf8(u8"\u25BC"), this);
		menu_button.setSizePolicy(QSizePolicy(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Minimum));
		menu_button.setFixedWidth(20);
		QLabel &static_text = *new QLabel(this);
		static_text.setSizePolicy(QSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum));
		addWidgetsToGrid(row++, { main_button, menu_button, static_text });

		// create the entry
		std::unique_ptr<InputEntry> entry_ptr;
		if (entry_desc.m_digital)
		{
			// a digital line
			assert(!entry_desc.m_analog_x && !entry_desc.m_analog_y);
			entry_ptr = std::make_unique<SingularInputEntry>(*this, main_button, menu_button, static_text, InputFieldRef(*entry_desc.m_digital), status::input_seq::type::STANDARD);
		}
		else
		{
			// an analog line
			entry_ptr = std::make_unique<MultiAxisInputEntry>(*this, main_button, menu_button, static_text, entry_desc.m_analog_x, entry_desc.m_analog_y);
		}
		InputEntry &entry = *entry_ptr;
		m_entries.push_back(std::move(entry_ptr));

		// update text
		entry.UpdateText();
	}

	// observe
	m_inputs_subscription = m_host.GetInputs().subscribe([this]() { OnInputsChanged(); });
	m_polling_seq_changed_subscription = m_host.GetPollingSeqChanged().subscribe([this]() { OnPollingSeqChanged(); });
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

InputsDialog::~InputsDialog()
{
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

void InputsDialog::StartInputPoll(const QString &label, const InputFieldRef &field_ref, status::input_seq::type seq_type, const QString &start_seq)
{
	// start polling
	m_host.StartPolling(field_ref.m_port_tag, field_ref.m_mask, seq_type, start_seq);

	// present the dialog
	SeqPollingDialog::Type dialogType = start_seq.isEmpty()
		? SeqPollingDialog::Type::SPECIFY
		: SeqPollingDialog::Type::ADD;
	SeqPollingDialog dialog(this, dialogType, label);
	m_current_dialog = &dialog;
	dialog.exec();
	m_current_dialog = nullptr;

	// stop polling (though this might have happened implicitly)
	m_host.StopPolling();

	// did the user select an input through the dialog?  if so, we need to
	// specify it (as opposed to MAME handling things)
	if (!dialog.DialogSelectedResult().isEmpty())
	{
		// assemble the tokens
		QString new_tokens = !start_seq.isEmpty()
			? start_seq + " or " + dialog.DialogSelectedResult()
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
		m_current_dialog->close();
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
			seqs.emplace_back(std::move(field_ref.m_port_tag), field_ref.m_mask, seq_type, "*");
	}

	SetInputSeqs(std::move(seqs));
}


//-------------------------------------------------
//  BuildCodes
//-------------------------------------------------

std::unordered_map<QString, QString> InputsDialog::BuildCodes(const std::vector<status::input_class> &devclasses)
{
	std::unordered_map<QString, QString> result;
	for (const status::input_class &devclass : devclasses)
	{
		// similar logic to devclass_string_table in MAME input.cpp
		QString devclass_name = GetDeviceClassName(devclass, true);

		// build codes for each device
		for (const status::input_device &dev : devclass.m_devices)
		{
			QString prefix = !devclass_name.isEmpty()
				? QString("%1 #%2 ").arg(devclass_name, QString::number(dev.m_index + 1))
				: QString();

			for (const status::input_device_item &item : dev.m_items)
			{
				QString label = prefix + item.m_name;
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

std::vector<InputsDialog::InputEntryDesc> InputsDialog::BuildInitialEntryDescriptions(status::input::input_class input_class) const
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
				QString::const_reverse_iterator iter;

				InputEntryDesc &entry = results.emplace_back();
				if (input.m_is_analog)
				{
					// is this an analog input that we feel should be presented as being an "X axis" or
					// a "Y axis" in our UI?
					iter = std::find_if(input.m_name.rbegin(), input.m_name.rend(), [](QChar ch)
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
						entry.m_aggregate_name = input.m_name.left(input.m_name.size() - (iter - input.m_name.rbegin()));
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

QString InputsDialog::GetDeviceClassName(const status::input_class &devclass, bool hide_single_keyboard)
{
	QString result;
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

QString InputsDialog::GetSeqTextFromTokens(const QString &seq_tokens)
{
	// this replicates logic in core MAME; need to more fully build this out, and perhaps
	// more fully dissect input sequences
	QString result;
	QStringList tokens = seq_tokens.split(' ');
	for (QString &token : tokens)
	{
		QString word;
		if (token == "OR" || token == "NOT" || token == "DEFAULT")
		{
			// modifier tokens; just "lowercase" it (this will need to be reevaluated when it 
			// is time to localize this
			word = token.toLower();
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
			if (!modifier.isEmpty())
			{
				// another thing that will need to be reevaluated when we want to localize
				modifier = modifier.left(1) + modifier.right(modifier.size() - 1).toLower();

				// append the "localized" modifier
				word += " ";
				word += modifier;
			}
		}

		if (!word.isEmpty())
		{
			if (!result.isEmpty())
				result += " ";
			result += word;
		}
	}

	// quick and dirty hack to trim out "OR" tokens at the start and end
	while (result.startsWith("or "))
		result = result.right(result.size() - 3);
	while (result.endsWith(" or"))
		result = result.left(result.size() - 3);
	if (result == "or")
		result = "";

	return result;
}


//-------------------------------------------------
//  ParseIndividualToken
//-------------------------------------------------

std::tuple<QString, QString> InputsDialog::ParseIndividualToken(QString &&token)
{
	// token parsing
	QString::iterator current_position = token.begin();
	auto next_token = [&current_position, &token]() -> QString::iterator
	{
		QString::iterator iter = std::find(current_position, token.end(), '_');
		current_position = iter;
		while (current_position < token.end() && *current_position == '_')
			current_position++;
		return iter;
	};

	// parse out the item part
	QString::iterator item_end = next_token();
	if (current_position < token.end() && *current_position >= '0' && *current_position <= '9')
		item_end = next_token();	// device number
	item_end = next_token();

	// do we have more?  if so, parse out the modifier
	QString modifier;
	if (item_end < token.end())
	{
		QString::iterator modifier_begin = current_position;
		QString::iterator modifier_end = next_token();
		modifier = token.mid(modifier_begin - token.begin(), modifier_end - modifier_begin);
		token.resize(item_end - token.begin());
	}

	return { std::move(token), std::move(modifier) };
}

