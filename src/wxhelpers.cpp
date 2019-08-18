/***************************************************************************

	wxhelpers.cpp

	Miscellaneous helper code to make wxWidgets friendlier to modern C++

***************************************************************************/

#include "wxhelpers.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  MenuWithResult ctor
//-------------------------------------------------

MenuWithResult::MenuWithResult()
	: m_result(0)
{
	Bind(wxEVT_MENU, [this](wxCommandEvent &evt) { m_result = evt.GetId(); });
}
