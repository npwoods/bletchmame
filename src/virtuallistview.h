/***************************************************************************

    virtuallistview.h

    Subclass of wxListView, providing handlers for wxLC_VIRTUAL overrides

***************************************************************************/

#pragma once

#ifndef VIRTUALLISTVIEW_H
#define VIRTUALLISTVIEW_H

#include <wx/listctrl.h>

class VirtualListView : public wxListView
{
public:
	VirtualListView(wxWindow *parent, wxWindowID winid = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
		long style = wxLC_REPORT | wxLC_VIRTUAL, const wxValidator& validator = wxDefaultValidator, const wxString &name = wxListCtrlNameStr);

	void SetOnGetItemText(std::function<wxString(long, long)> &&func) { m_func_on_get_item_text = func; }

protected:
	virtual wxString OnGetItemText(long item, long column) const override;

private:
	std::function<wxString(long, long)> m_func_on_get_item_text;
};

#endif // VIRTUALLISTVIEW_H
