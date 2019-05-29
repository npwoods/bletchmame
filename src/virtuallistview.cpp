/***************************************************************************

    virtuallistview.cpp

    Subclass of wxListView, providing handlers for wxLC_VIRTUAL overrides

***************************************************************************/

#include "virtuallistview.h"

VirtualListView::VirtualListView(wxWindow *parent, wxWindowID winid, const wxPoint& pos, const wxSize& size,
		long style, const wxValidator& validator, const wxString &name)
	: wxListView(parent, winid, pos, size, style, validator, name)
{
	assert(style & wxLC_VIRTUAL);
}


wxString VirtualListView::OnGetItemText(long item, long column) const
{
	return m_func_on_get_item_text(item, column);
}