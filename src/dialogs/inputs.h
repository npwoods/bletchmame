/***************************************************************************

    dialogs/inputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_H
#define DIALOGS_INPUTS_H

#include <vector>
#include <observable/observable.hpp>

#include "dialogs/inputs_base.h"
#include "runmachinetask.h"


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

	struct InputEntryDesc;
	struct InputFieldRef;
	class InputEntry;
	
	class SingularInputEntry;
	class MultiAxisInputEntry;

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
	QString GetSeqTextFromTokens(const QString &seq_tokens);
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
