/***************************************************************************

    dialogs/inputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_INPUTS_H
#define DIALOGS_INPUTS_H

#include <vector>
#include <observable/observable.hpp>

#include "runmachinetask.h"
#include "status.h"

class wxWindow;

struct SetInputSeqRequest
{
	SetInputSeqRequest()	{ }
	SetInputSeqRequest(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, const wxString &tokens)
		: m_port_tag(port_tag)
		, m_mask(mask)
		, m_seq_type(seq_type)
		, m_tokens(tokens)
	{
	}

	wxString				m_port_tag;
	ioport_value			m_mask;
	status::input_seq::type	m_seq_type;
	wxString				m_tokens;
};

class IInputsHost
{
public:
	virtual observable::value<std::vector<status::input>> &GetInputs() = 0;
	virtual const std::vector<status::input_class> &GetInputClasses() = 0;
	virtual void SetInputSeqs(std::vector<SetInputSeqRequest> &&seqs) = 0;
	virtual observable::value<bool> &GetPollingSeqChanged() = 0;
	virtual void StartPolling(const wxString &port_tag, ioport_value mask, status::input_seq::type seq_type, const wxString &start_seq_tokens) = 0;
	virtual void StopPolling() = 0;
};

bool show_inputs_dialog(wxWindow &parent, const wxString &title, IInputsHost &host, status::input::input_class input_class);

#endif // DIALOGS_INPUTS_H
