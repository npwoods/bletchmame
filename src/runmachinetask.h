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
	bool				m_paused;
	bool				m_paused_specified;
	wxString			m_frameskip;
	bool				m_frameskip_specified;
    wxString			m_speed_text;
	bool				m_speed_text_specified;
	bool				m_throttled;
	bool				m_throttled_specified;
	float				m_throttle_rate;
	bool				m_throttle_rate_specified;
};

wxDECLARE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDECLARE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);


class RunMachineTask : public Task
{
public:
    RunMachineTask(wxString &&machine_name, wxString &&target);

    void Post(std::string &&command);

protected:
	virtual std::vector<wxString> GetArguments(const Preferences &prefs) const override;
	virtual void Process(wxProcess &process, wxEvtHandler &handler) override;
	virtual void Abort() override;
	virtual void OnChildProcessCompleted(emu_error status) override;
	virtual void OnChildProcessKilled() override;

private:
    struct Message
    {
		enum class type
		{
			INVALID,
			COMMAND,
			TERMINATED
		};

		type						m_type;
        std::string	                m_command;
		emu_error					m_status;
    };

	wxString						m_machine_name;
	wxString						m_target;
    wxMessageQueue<Message>         m_message_queue;

	void InternalPost(Message::type type, std::string &&command, emu_error status = emu_error::INVALID);
    static bool ReadStatusUpdate(wxTextInputStream &input, StatusUpdate &result);
	void ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input);
};


#endif // RUNMACHINETASK_H
