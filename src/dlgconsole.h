/***************************************************************************

    dlgconsole.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DLGCONSOLE_H
#define DLGCONSOLE_H

#include <wx/window.h>
#include "client.h"

struct Chatter;

class IConsoleDialogHost
{
public:
	virtual void SetChatterListener(std::function<void(Chatter &&chatter)> &&func) = 0;
};

bool show_console_dialog(wxWindow &parent, MameClient &client, IConsoleDialogHost &host);

#endif // DLGCONSOLE_H
