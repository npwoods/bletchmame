/***************************************************************************

    runmachinetask.h

    Task for running an emulation session

***************************************************************************/

#pragma once

#ifndef RUNMACHINETASK_H
#define RUNMACHINETASK_H

#include <wx/msgqueue.h>
#include <optional>

#include "task.h"
#include "info.h"
#include "payloadevent.h"


//**************************************************************************
//  MACROS
//**************************************************************************

// workaround for GCC bug fixed in 7.4
#ifdef __GNUC__
#if __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#define SHOULD_BE_DELETE	default
#endif	// __GNUC__ < 7 || (__GNUC__ == 7 && (__GNUC_MINOR__ < 4))
#endif // __GNUC__

#ifndef SHOULD_BE_DELETE
#define SHOULD_BE_DELETE	delete
#endif // !SHOULD_BE_DELETE


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
	Image(const Image &) = SHOULD_BE_DELETE;
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
	InputSeq(const InputSeq &that) = SHOULD_BE_DELETE;
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

typedef std::uint32_t ioport_value;

struct Input
{
	Input() = default;
	Input(const Input &) = SHOULD_BE_DELETE;
	Input(Input &&that) = default;

	enum class input_type
	{
		ANALOG,
		DIGITAL
	};

	wxString				m_port_tag;
	wxString				m_name;
	ioport_value			m_mask;
	input_type				m_type;
	std::vector<InputSeq>	m_seqs;
};

struct StatusUpdate
{
	StatusUpdate() = default;
	StatusUpdate(const StatusUpdate &that) = delete;
	StatusUpdate(StatusUpdate &&that) = default;

	// did we have problems reading the response from MAME?
	bool								m_success;
	wxString							m_parse_error;

	// the actual data
	std::optional<bool>					m_paused;
	std::optional<bool>					m_polling_input_seq;
	std::optional<wxString>				m_frameskip;
	std::optional<wxString>				m_speed_text;
	std::optional<bool>					m_throttled;
	std::optional<float>				m_throttle_rate;
	std::optional<int>					m_sound_attenuation;
	std::optional<std::vector<Image>>	m_images;
	std::optional<std::vector<Input>>	m_inputs;
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
	RunMachineTask(info::machine machine, wxWindow &target_window);

	void Issue(const std::vector<wxString> &args);
	void IssueFullCommandLine(const wxString &full_command);
	const info::machine &GetMachine() const { return m_machine; }
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

	info::machine					m_machine;
	std::uintptr_t					m_target_window;
	wxMessageQueue<Message>         m_message_queue;
	volatile bool					m_chatter_enabled;

	void InternalPost(Message::type type, wxString &&command, emu_error status = emu_error::INVALID);
	static StatusUpdate ReadStatusUpdate(wxTextInputStream &input);
	void ReceiveResponse(wxEvtHandler &handler, wxTextInputStream &input);
	void PostChatter(wxEvtHandler &handler, Chatter::chatter_type type, wxString &&text);
};


#endif // RUNMACHINETASK_H
