/***************************************************************************

    dialogs/paths.h

    Paths dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_PATHS_H
#define DIALOGS_PATHS_H

#include "prefs.h"

#include <vector>

std::vector<Preferences::path_type> show_paths_dialog(wxWindow &parent, Preferences &prefs);
wxString show_specify_single_path_dialog(wxWindow &parent, Preferences::path_type type, const wxString &default_path);

#endif // DIALOGS_PATHS_H
