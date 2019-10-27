/***************************************************************************

	dialogs/choosesw.h

	Choose Software dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CHOOSESW_H
#define DIALOGS_CHOOSESW_H

#include "softwarelist.h"
#include "softlistview.h"

class Preferences;

wxString show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const software_list_collection &software_col, const wxString &dev_interface);


#endif // DIALOGS_CHOOSESW_H
