/***************************************************************************

    runmachinetask.h

    Task for running an emulation session

***************************************************************************/

#pragma once

#ifndef RUNMACHINETASK_H
#define RUNMACHINETASK_H

#include <wx/msgqueue.h>

#include "task.h"
#include "payloadevent.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

struct RunMachineResult
{
    bool        m_success;
    wxString    m_error_message;
};

struct StatusUpdate
{
    wxString    m_speed_text;
};

wxDECLARE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDECLARE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);


class RunMachineTask : public Task
{
public:
    RunMachineTask(std::string &&machine_name, std::string &&target);

    virtual std::string Arguments() override;
    virtual void Process(wxProcess &process, wxEvtHandler &handler) override;

    void Post(std::string &&command, bool exit = false);

private:
    struct Message
    {
        std::string                 m_command;
        bool                        m_exit;
    };

    std::string                     m_machine_name;
    std::string                     m_target;
    wxMessageQueue<Message>         m_message_queue;

    static bool ReadStatusUpdate(wxTextInputStream &input, StatusUpdate &result);
    static bool ReadXmlFromInputStream(wxXmlDocument &xml, wxTextInputStream &input);
};


#endif // RUNMACHINETASK_H
