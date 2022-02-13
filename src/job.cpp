/***************************************************************************

    job.cpp

    Wrappers for Win32 jobs, so we properly clean up

***************************************************************************/

// Qt headers
#include "job.h"

// windows-specific headers
#ifdef WIN32
#include <windows.h>
#endif


//**************************************************************************
//  WIN32 IMPLEMENTATION
//**************************************************************************

#ifdef WIN32

//-------------------------------------------------
//  ctor
//-------------------------------------------------

Win32Job::Win32Job()
{
	m_handle = CreateJobObjectW(nullptr, nullptr);
	AssignProcessToJobObject(m_handle, GetCurrentProcess());

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
	info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	SetInformationJobObject(m_handle, JobObjectExtendedLimitInformation, &info, sizeof(info));
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

Win32Job::~Win32Job()
{
	if (m_handle)
		CloseHandle(m_handle);
}


//-------------------------------------------------
//  AddProcess
//-------------------------------------------------

void Win32Job::addProcess(qint64 processId)
{
	HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)processId);
	AssignProcessToJobObject(m_handle, process_handle);
	CloseHandle(process_handle);
}
#endif // WIN32
