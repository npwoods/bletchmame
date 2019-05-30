/***************************************************************************

    virtuallistview.cpp

    Subclass of wxListView, providing handlers for wxLC_VIRTUAL overrides

***************************************************************************/

#include "virtuallistview.h"


//-------------------------------------------------
//  ctor
//-------------------------------------------------

VirtualListView::VirtualListView(wxWindow *parent, wxWindowID winid, const wxPoint& pos, const wxSize& size,
		long style, const wxValidator& validator, const wxString &name)
	: wxListView(parent, winid, pos, size, style, validator, name)
{
	assert(style & wxLC_VIRTUAL);
}


//-------------------------------------------------
//  OnGetItemText
//-------------------------------------------------

wxString VirtualListView::OnGetItemText(long item, long column) const
{
	return m_func_on_get_item_text
		? m_func_on_get_item_text(item, column)
		: wxString(wxEmptyString);
}


//-------------------------------------------------
//  OnGetItemAttr
//-------------------------------------------------

wxListItemAttr *VirtualListView::OnGetItemAttr(long item) const
{
	return m_func_on_get_item_attr
		? m_func_on_get_item_attr(item)
		: nullptr;
}
