/***************************************************************************

    dialogs/switches.h

    Configuration & DIP Switches customization dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_SWITCHES_H
#define DIALOGS_SWITCHES_H

#include <vector>

#include "runmachinetask.h"
#include "info.h"
#include "status.h"


class wxWindow;
struct Input;

class ISwitchesHost
{
public:
	virtual const std::vector<status::input> &GetInputs() = 0;
	virtual void SetInputValue(const wxString &port_tag, ioport_value mask, ioport_value value) = 0;
};


bool show_switches_dialog(wxWindow &parent, const wxString &title, ISwitchesHost &host, status::input::input_class input_class, info::machine machine);

#endif // DIALOGS_SWITCHES_H
