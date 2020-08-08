/***************************************************************************

    dialogs/inputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_H
#define DIALOGS_INPUTS_H

#include <vector>
#include <observable/observable.hpp>

#include <QHashFunctions>

#include "dialogs/inputs_base.h"
#include "runmachinetask.h"

QT_BEGIN_NAMESPACE
class QLabel;
class QMenu;
class QPushButton;
QT_END_NAMESPACE


// ======================> SetInputSeqRequest

struct SetInputSeqRequest
{
	SetInputSeqRequest() : m_mask(0), m_seq_type(status::input_seq::type::STANDARD)	{ }
	SetInputSeqRequest(const QString &port_tag, ioport_value mask, status::input_seq::type seq_type, const QString &tokens)
		: m_port_tag(port_tag)
		, m_mask(mask)
		, m_seq_type(seq_type)
		, m_tokens(tokens)
	{
	}
	SetInputSeqRequest(const QString &port_tag, ioport_value mask, status::input_seq::type seq_type, QString &&tokens)
		: m_port_tag(port_tag)
		, m_mask(mask)
		, m_seq_type(seq_type)
		, m_tokens(std::move(tokens))
	{
	}

	QString					m_port_tag;
	ioport_value			m_mask;
	status::input_seq::type	m_seq_type;
	QString					m_tokens;
};


// ======================> IInputsHost

class IInputsHost
{
public:
	virtual observable::value<std::vector<status::input>> &GetInputs() = 0;
	virtual const std::vector<status::input_class> &GetInputClasses() = 0;
	virtual void SetInputSeqs(std::vector<SetInputSeqRequest> &&seqs) = 0;
	virtual observable::value<bool> &GetPollingSeqChanged() = 0;
	virtual void StartPolling(const QString &port_tag, ioport_value mask, status::input_seq::type seq_type, const QString &start_seq_tokens) = 0;
	virtual void StopPolling() = 0;
};


// ======================> InputsDialog

class InputsDialog : public InputsDialogBase
{
public:
	class Test;

	InputsDialog(QWidget *parent, IInputsHost &host, status::input::input_class input_class);
	~InputsDialog();

protected:
	virtual void OnRestoreButtonPressed() override;

private:
	enum class axis_type
	{
		NONE,
		X,
		Y,
		Z
	};

	struct InputFieldRef
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

	// ======================> QuickItem
	struct QuickItem
	{
		QString							m_label;
		std::vector<SetInputSeqRequest>	m_selections;
	};


	// ======================> InputEntry
	// abstract base class for lines/entries in the input dialog
	class InputEntry
	{
	public:
		InputEntry(InputsDialog &host, QPushButton &main_button, QPushButton &menu_button, QLabel &static_text);
		virtual ~InputEntry();

		void UpdateText();
		virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() = 0;

	protected:
		// overridden by child classes
		virtual QString GetText() = 0;
		virtual void OnMainButtonPressed() = 0;
		virtual bool OnMenuButtonPressed() = 0;

		// accessors
		InputsDialog &Host() { return m_host; }
		QPushButton &MainButton() { return m_main_button; }

		// methods
		bool PopupMenu(QMenu &popup_menu);
		std::vector<QuickItem> BuildQuickItems(const std::optional<InputFieldRef> &x_field_ref, const std::optional<InputFieldRef> &y_field_ref, const std::optional<InputFieldRef> &all_axes_field_ref);
		void InvokeQuickItem(QuickItem &&quick_item);
		bool ShowMultipleQuickItemsDialog(std::vector<QuickItem>::const_iterator first, std::vector<QuickItem>::const_iterator last);

	private:
		InputsDialog &m_host;
		QPushButton &m_main_button;
		QPushButton &m_menu_button;
		QLabel &m_static_text;
	};

	// ======================> SingularInputEntry
	class SingularInputEntry : public InputEntry
	{
	public:
		SingularInputEntry(InputsDialog &host, QPushButton &main_button, QPushButton &menu_button, QLabel &static_text, InputFieldRef &&field_ref, status::input_seq::type seq_type);

		virtual std::vector<std::tuple<InputFieldRef, status::input_seq::type>> GetInputSeqRefs() override;

	protected:
		virtual QString GetText() override;
		virtual void OnMainButtonPressed() override;
		virtual bool OnMenuButtonPressed() override;

	private:
		InputFieldRef			m_field_ref;
		status::input_seq::type	m_seq_type;
	};

	struct InputEntryDesc;

	class SingularInputEntry;
	class MultiAxisInputEntry;

	class MultiAxisInputDialog;
	class MultipleQuickItemsDialog;
	class SeqPollingDialog;

	IInputsHost &								m_host;
	QDialog *									m_current_dialog;
	std::unordered_map<QString, QString>		m_codes;
	std::vector<std::unique_ptr<InputEntry>>	m_entries;
	observable::unique_subscription				m_inputs_subscription;
	observable::unique_subscription				m_polling_seq_changed_subscription;

	static axis_type AxisType(const status::input_device_item &item);
	const status::input_seq &FindInputSeq(const InputFieldRef &field_ref, status::input_seq::type seq_type);
	void StartInputPoll(const QString &label, const InputFieldRef &field_ref, status::input_seq::type seq_type, const QString &start_seq = "");
	void OnInputsChanged();
	void OnPollingSeqChanged();
	static std::unordered_map<QString, QString> InputsDialog::BuildCodes(const std::vector<status::input_class> &devclasses);
	static bool CompareInputs(const status::input &a, const status::input &b);
	std::vector<InputEntryDesc> BuildInitialEntryDescriptions(status::input::input_class input_class) const;
	static QString GetDeviceClassName(const status::input_class &devclass, bool hide_single_keyboard);
	QString GetSeqTextFromTokens(const QString &seq_tokens) const;
	static QString GetSeqTextFromTokens(const QString &seq_tokens, const std::unordered_map<QString, QString> &codes);
	static std::tuple<QString, QString> ParseIndividualToken(QString &&token);

	// trampolines
	void SetInputSeqs(std::vector<SetInputSeqRequest> &&seqs)
	{
		m_host.SetInputSeqs(std::move(seqs));
	}
	observable::value<std::vector<status::input>> &GetInputs()
	{
		return m_host.GetInputs();
	}
};


#endif // DIALOGS_INPUTS_H
