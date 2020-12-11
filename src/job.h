/***************************************************************************

    job.h

    Wrappers for Win32 jobs, so we properly clean up

***************************************************************************/

#pragma once

#ifndef JOB_H
#define JOB_H

#include <QProcess>

#ifdef WIN32
#include <windows.h>

// ======================> Win32Job

class Win32Job
{
public:
	Win32Job();
    ~Win32Job()
    {
        if (m_handle)
            CloseHandle(m_handle);
    }

    void AddProcess(qint64 process_id);

private:
    HANDLE          m_handle;
};

#endif // WIN32


// ======================> NullJob

class NullJob
{
public:
    void AddProcess(qint64 process_id) { (void)process_id; }
};


// ======================> Job
#ifdef WIN32
typedef Win32Job Job;
#else
typedef NullJob Job;
#endif

#endif // JOB_H
