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

std::optional<int> show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const std::vector<SoftwareAndPart> &parts);


#endif // DIALOGS_CHOOSESW_H
