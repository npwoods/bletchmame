/***************************************************************************

    dlginputs.h

    Input customization dialog

***************************************************************************/

#pragma once

#ifndef DLGINPUTS_H
#define DLGINPUTS_H

#include <vector>
#include <observable/observable.hpp>

#include "runmachinetask.h"

class wxWindow;
struct Input;

class IInputsHost
{
public:
	virtual const std::vector<Input> &GetInputs() = 0;
	virtual observable::value<bool> &GetPollingSeqChanged() = 0;
	virtual void StartPolling(const wxString &port_tag, ioport_value mask, InputSeq::inputseq_type seq_type) = 0;
	virtual void StopPolling() = 0;
};

bool show_inputs_dialog(wxWindow &parent, IInputsHost &host);

#endif // DLGINPUTS_H
