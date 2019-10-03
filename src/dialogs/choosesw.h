/***************************************************************************

	dialogs/choosesw.h

	Choose Software dialog

***************************************************************************/

#pragma once

#ifndef DIALOGS_CHOOSESW_H
#define DIALOGS_CHOOSESW_H

#include "softwarelist.h"

class Preferences;

class SoftwareAndPart
{
public:
	SoftwareAndPart(const software_list::software &sw, const software_list::part &p)
		: m_software(sw)
		, m_part(p)
	{
	}

	const software_list::software &software() const { return m_software; }
	const software_list::part &part() const { return m_part; }

private:
	const software_list::software &m_software;
	const software_list::part &m_part;
};


std::optional<int> show_choose_software_dialog(wxWindow &parent, Preferences &prefs, const wxString &machine, const std::vector<SoftwareAndPart> &parts);


#endif // DIALOGS_CHOOSESW_H
