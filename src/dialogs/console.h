/***************************************************************************

    dialogs/console.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DIALOGS_CONSOLE_H
#define DIALOGS_CONSOLE_H

#include <wx/window.h>
#include "client.h"

struct Chatter;

class IConsoleDialogHost
{
public:
	virtual void SetChatterListener(std::function<void(Chatter &&chatter)> &&func) = 0;
};

bool show_console_dialog(wxWindow &parent, MameClient &client, IConsoleDialogHost &host);

#endif // DIALOGS_CONSOLE_H
