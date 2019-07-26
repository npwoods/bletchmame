/***************************************************************************

    status.h

    Tracking of MAME internal state updates

***************************************************************************/

#pragma once

#ifndef STATUS_H
#define STATUS_H

#include <observable/observable.hpp>
#include <wx/string.h>
#include <optional>

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

class wxTextInputStream;

typedef std::uint32_t ioport_value;

namespace status
{
	// ======================> image
	struct image
	{
		image() = default;
		image(const image &) = SHOULD_BE_DELETE;
		image(image &&that) = default;

		wxString			m_tag;
		wxString			m_instance_name;
		bool				m_is_readable;
		bool				m_is_writeable;
		bool				m_is_creatable;
		bool				m_must_be_loaded;
		wxString			m_file_name;
		wxString			m_display;

		image &operator=(image &&that) = default;
		bool operator==(const image &that) const;
	};


	// ======================> input_seq
	struct input_seq
	{
		input_seq() = default;
		input_seq(const input_seq &that) = SHOULD_BE_DELETE;
		input_seq(input_seq &&that) = default;

		enum class type
		{
			STANDARD,
			INCREMENT,
			DECREMENT
		};

		type					m_type;
		wxString				m_text;

		bool operator==(const input_seq &that) const;
	};


	// ======================> input
	struct input
	{
		input() = default;
		input(const input &) = SHOULD_BE_DELETE;
		input(input &&that) = default;

		enum class input_class
		{
			UNKNOWN,
			CONTROLLER,
			KEYBOARD,
			MISC,
			CONFIG,
			DIPSWITCH
		};

		enum class input_type
		{
			ANALOG,
			DIGITAL
		};

		wxString				m_port_tag;
		wxString				m_name;
		ioport_value			m_mask;
		input_class				m_class;
		input_type				m_type;
		ioport_value			m_value;
		std::vector<input_seq>	m_seqs;

		input &operator=(input &&that) = default;
		bool operator==(const input &that) const;
	};


	// ======================> machine_phase
	enum class machine_phase
	{
		PREINIT,
		INIT,
		RESET,
		RUNNING,
		EXIT
	};


	// ======================> update
	struct update
	{
		update();
		update(const update &that) = delete;
		update(update &&that) = default;
		~update();

		// did we have problems reading the response from MAME?
		bool								m_success;
		wxString							m_parse_error;

		// the actual data
		std::optional<machine_phase>		m_phase;
		std::optional<bool>					m_paused;
		std::optional<bool>					m_polling_input_seq;
		std::optional<wxString>				m_startup_text;
		std::optional<float>				m_speed_percent;
		std::optional<wxString>				m_frameskip;
		std::optional<int>					m_effective_frameskip;
		std::optional<bool>					m_throttled;
		std::optional<float>				m_throttle_rate;
		std::optional<int>					m_sound_attenuation;
		std::optional<std::vector<image>>	m_images;
		std::optional<std::vector<input>>	m_inputs;

		static update read(wxTextInputStream &input);
	};


	// ======================> state
	class state
	{
	public:
		// ctor/dtor
		state();
		state(const state &that) = delete;
		state(state &&that) = default;
		~state();

		// update with new state
		void update(update &&that);

		// state accessors
		observable::value<machine_phase> &		phase()						{ return m_phase; }
		observable::value<bool> &				paused()					{ return m_paused; }
		observable::value<bool> &				polling_input_seq()			{ return m_polling_input_seq; }
		observable::value<wxString> &			startup_text()				{ return m_startup_text; }
		observable::value<float> &				speed_percent()				{ return m_speed_percent; }
		observable::value<int> &				effective_frameskip()		{ return m_effective_frameskip; }
		observable::value<std::vector<image>> &	images()					{ return m_images; }
		observable::value<std::vector<input>> &	inputs()					{ return m_inputs; }
		wxString								frameskip() const			{ return m_frameskip; }
		bool									throttled() const			{ return m_throttled; }
		float									throttle_rate() const		{ return m_throttle_rate; }
		int										sound_attenuation() const	{ return m_sound_attenuation; }

		// higher level methods
		const image *find_image(const wxString &tag) const;
		bool has_input_class(status::input::input_class input_class) const;

	private:
		observable::value<machine_phase>		m_phase;
		observable::value<bool>					m_paused;
		observable::value<bool>					m_polling_input_seq;
		observable::value<wxString>				m_startup_text;
		observable::value<float>				m_speed_percent;
		observable::value<int>					m_effective_frameskip;
		observable::value<std::vector<image>>	m_images;
		observable::value<std::vector<input>>	m_inputs;
		wxString								m_frameskip;
		bool									m_throttled;
		float									m_throttle_rate;
		int										m_sound_attenuation;

		template<typename TStateField, typename TUpdateField>
		bool take(TStateField &state_field, std::optional<TUpdateField> &update_field);
	};
};

#endif // STATUS_H
