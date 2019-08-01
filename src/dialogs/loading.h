/***************************************************************************

	dialogs/loading.h

	"loading MAME Info..." modal dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_LOADING_H
#define DIALOGS_LOADING_H

#include <wx/window.h>
#include <functional>

bool show_loading_mame_info_dialog(wxWindow &parent, const std::function<bool()> &poll_completion_check);

#endif // DIALOGS_LOADING_H
