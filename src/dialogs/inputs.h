/***************************************************************************

    dialogs/inputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_H
#define DIALOGS_INPUTS_H

#include <QDialog>
#include <vector>
#include <observable/observable.hpp>

#include "runmachinetask.h"
#include "status.h"

QT_BEGIN_NAMESPACE
namespace Ui { class InputsDialog; }
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

class InputsDialog : public QDialog
{
	Q_OBJECT
public:
	InputsDialog(QWidget &parent, const QString &title, IInputsHost &host, status::input::input_class input_class);
	~InputsDialog();

private:
	std::unique_ptr<Ui::InputsDialog>	m_ui;
};


#endif // DIALOGS_INPUTS_H
