/***************************************************************************

    dlgpaths.h

    Paths dialog

***************************************************************************/

#pragma once

#ifndef DLGPATHS_H
#define DLGPATHS_H

#include "prefs.h"

bool show_paths_dialog(Preferences &prefs);
wxString show_specify_single_path_dialog(wxWindow &parent, Preferences::path_type type, const wxString &default_path);

#endif // DLGPATHS_H
