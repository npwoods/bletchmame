/***************************************************************************

    job.h

    Wrappers for Win32 jobs, so we properly clean up

***************************************************************************/

#pragma once

#ifndef JOB_H
#define JOB_H

// Qt headers
#include <QProcess>

#ifdef WIN32
#include <windows.h>

// ======================> Win32Job

class Win32Job
{
public:
	Win32Job();
    ~Win32Job();

    void addProcess(qint64 processId);

private:
    HANDLE          m_handle;
};

#endif // WIN32


// ======================> NullJob

class NullJob
{
public:
    void addProcess(qint64 processId) { (void)processId; }
};


// ======================> Job
#ifdef WIN32
typedef Win32Job Job;
#else
typedef NullJob Job;
#endif

#endif // JOB_H
