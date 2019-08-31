/***************************************************************************

	wxhelpers.cpp

	Miscellaneous helper code to make wxWidgets friendlier to modern C++

***************************************************************************/

#include "wxhelpers.h"

#include <windows.h>


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


//-------------------------------------------------
//  WindowHasMenuBar
//-------------------------------------------------

bool WindowHasMenuBar(wxWindow &window)
{
#ifdef WIN32
	// Win32 specific code
	return ::GetMenu(window.GetHWND()) != nullptr;
#else
	throw false;
#endif
}
