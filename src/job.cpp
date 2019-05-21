#include "job.h"

Win32Job::Win32Job()
{
    m_handle = CreateJobObjectW(nullptr, nullptr);
    AssignProcessToJobObject(m_handle, GetCurrentProcess());

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(m_handle, JobObjectExtendedLimitInformation, &info, sizeof(info));
}


void Win32Job::AddProcess(long process_id)
{
    HANDLE process_handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
    AssignProcessToJobObject(m_handle, process_handle);
    CloseHandle(process_handle);
}

