/***************************************************************************

	focuswatchinghook.h

	Windows-specific functionality for monitoring Win32 focus changes

***************************************************************************/

#ifndef FOCUSWATCHINGHOOK_H
#define FOCUSWATCHINGHOOK_H

// Qt headers
#include <QWidget>
#include <QtGlobal>

// windows headers
#ifdef Q_OS_WINDOWS
#include <windows.h>
#endif


// ======================> WinFocusWatchingHook

#ifdef Q_OS_WINDOWS
class WinFocusWatchingHook : public QObject
{
	Q_OBJECT
public:
	WinFocusWatchingHook(QObject *parent = nullptr);
	~WinFocusWatchingHook();

signals:
	void winFocusChanged(WId newWindow, WId oldWindow);

private:
	static WinFocusWatchingHook *	s_instance;
	HHOOK							m_hook;

	// static methods
	static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
};
#endif // Q_OS_WINDOWS

#endif // FOCUSWATCHINGHOOK_H
