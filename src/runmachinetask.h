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

struct Image
{
	wxString			m_tag;
	wxString			m_instance_name;
	bool				m_is_readable;
	bool				m_is_writeable;
	bool				m_is_createable;
	bool				m_must_be_loaded;
	wxString			m_file_name;

	bool operator==(const Image &that) const
	{
		return m_tag			== that.m_tag
			&& m_instance_name	== that.m_instance_name
			&& m_is_readable	== that.m_is_readable
			&& m_is_writeable	== that.m_is_writeable
			&& m_is_createable	== that.m_is_createable
			&& m_must_be_loaded	== that.m_must_be_loaded
			&& m_file_name		== that.m_file_name;
	}
};

struct StatusUpdate
{
	// did we have problems reading the response from MAME?
	bool				m_success;
	wxString			m_parse_error;

	// the actual data
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
	std::vector<Image>	m_images;
	bool				m_images_specified;
};

wxDECLARE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDECLARE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);

struct Machine;

class RunMachineTask : public Task
{
public:
	RunMachineTask(const Machine &machine, wxWindow &target_window);

	void Issue(const std::vector<wxString> &args);
	const Machine &GetMachine() const { return m_machine; }

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
		wxString	                m_command;
		emu_error					m_status;
    };

	const Machine &					m_machine;
	std::uintptr_t					m_target_window;
	wxMessageQueue<Message>         m_message_queue;

	void InternalPost(Message::type type, wxString &&command, emu_error status = emu_error::INVALID);
	static StatusUpdate ReadStatusUpdate(wxTextInputStream &input);
	void ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input);
};


#endif // RUNMACHINETASK_H
