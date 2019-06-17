/***************************************************************************

    dlginvoke.h

    Arbitrary command invocation dialog (essentially a development feature)

***************************************************************************/

#pragma once

#ifndef DLGINVOKE_H
#define DLGINVOKE_H

#include <wx/window.h>
#include "client.h"

bool show_invoke_arbitrary_command_dialog(wxWindow &parent, MameClient &client);

#endif // DLGINVOKE_H
