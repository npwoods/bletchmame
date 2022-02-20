/***************************************************************************

	focuswatchinghook.cpp

	Windows-specific functionality for monitoring Win32 focus changes

***************************************************************************/

// bletchmame headers
#include "focuswatchinghook.h"

#ifdef Q_OS_WINDOWS
WinFocusWatchingHook *WinFocusWatchingHook::s_instance;

//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

WinFocusWatchingHook::WinFocusWatchingHook(QObject *parent)
	: QObject(parent)
{
	// set the instance; there can be only one
	assert(!s_instance);
	s_instance = this;

	// and create the hook
	m_hook = ::SetWindowsHookEx(WH_CBT, hookProc, nullptr, ::GetCurrentThreadId());
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

WinFocusWatchingHook::~WinFocusWatchingHook()
{
	s_instance = nullptr;
	::UnhookWindowsHookEx(m_hook);
}



//-------------------------------------------------
//  dtor
//-------------------------------------------------

LRESULT CALLBACK WinFocusWatchingHook::hookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HCBT_SETFOCUS)
	{
		WId newWindow = (WId)wParam;
		WId oldWindow = (WId)lParam;
		emit s_instance->winFocusChanged(newWindow, oldWindow);
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}


#endif // Q_OS_WINDOWS
