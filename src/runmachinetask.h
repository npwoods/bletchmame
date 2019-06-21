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
	RunMachineResult() = default;
	RunMachineResult(const RunMachineResult &that) = delete;
	RunMachineResult(RunMachineResult &&that) = default;

	bool        m_success;
    wxString    m_error_message;
};

struct Image
{
	Image() = default;
	Image(const Image &that) = delete;
	Image(Image &&that) = default;

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

struct InputSeq
{
	InputSeq() = default;
	InputSeq(const InputSeq &that) = delete;
	InputSeq(InputSeq &&that) = default;

	enum class inputseq_type
	{
		STANDARD,
		INCREMENT,
		DECREMENT
	};

	inputseq_type			m_type;
	wxString				m_text;
};

struct Input
{
	Input() = default;
	Input(const Input &that) = delete;
	Input(Input &&that) = default;

	enum class input_type
	{
		ANALOG,
		DIGITAL
	};

	wxString				m_port_tag;
	wxString				m_name;
	int						m_mask;
	input_type				m_type;
	std::vector<InputSeq>	m_seqs;
};

struct StatusUpdate
{
	StatusUpdate() = default;
	StatusUpdate(const StatusUpdate &that) = delete;
	StatusUpdate(StatusUpdate &&that) = default;

	// did we have problems reading the response from MAME?
	bool				m_success;
	wxString			m_parse_error;

	// the actual data
	bool				m_paused;
	bool				m_paused_specified;
	bool				m_polling_input_seq;
	bool				m_polling_input_seq_specified;
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
	std::vector<Input>	m_inputs;
	bool				m_inputs_specified;
};

struct Chatter
{
	Chatter() = default;
	Chatter(const Chatter &that) = delete;
	Chatter(Chatter &&that) = default;

	enum class chatter_type
	{
		COMMAND_LINE,
		RESPONSE
	};

	chatter_type		m_type;
	wxString			m_text;
};

wxDECLARE_EVENT(EVT_RUN_MACHINE_RESULT, PayloadEvent<RunMachineResult>);
wxDECLARE_EVENT(EVT_STATUS_UPDATE, PayloadEvent<StatusUpdate>);
wxDECLARE_EVENT(EVT_CHATTER, PayloadEvent<Chatter>);

struct Machine;

class RunMachineTask : public Task
{
public:
	RunMachineTask(const Machine &machine, wxWindow &target_window);

	void Issue(const std::vector<wxString> &args);
	void IssueFullCommandLine(const wxString &full_command);
	const Machine &GetMachine() const { return m_machine; }
	void SetChatterEnabled(bool enabled) { m_chatter_enabled = enabled; }

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
	volatile bool					m_chatter_enabled;

	void InternalPost(Message::type type, wxString &&command, emu_error status = emu_error::INVALID);
	static StatusUpdate ReadStatusUpdate(wxTextInputStream &input);
	void ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input);
	void PostChatter(wxEvtHandler &handler, Chatter::chatter_type type, wxString &&text);
};


#endif // RUNMACHINETASK_H
