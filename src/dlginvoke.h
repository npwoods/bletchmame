/***************************************************************************

    dlginvoke.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DLGINVOKE_H
#define DLGINVOKE_H

#include <wx/window.h>
#include "client.h"

struct Chatter;

class IInvokeArbitraryCommandDialogHost
{
public:
	virtual void SetChatterListener(std::function<void(Chatter &&chatter)> &&func) = 0;
};

bool show_invoke_arbitrary_command_dialog(wxWindow &parent, MameClient &client, IInvokeArbitraryCommandDialogHost &host);

#endif // DLGINVOKE_H
