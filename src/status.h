/***************************************************************************

    status.h

    Tracking of MAME internal state updates

***************************************************************************/

#pragma once

#ifndef STATUS_H
#define STATUS_H

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
		bool				m_is_createable;
		bool				m_must_be_loaded;
		wxString			m_file_name;
		wxString			m_display;

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

		bool operator==(const input &that) const;
	};


	// ======================> update
	struct update
	{
		update() = default;
		update(const update &that) = delete;
		update(update &&that) = default;

		// did we have problems reading the response from MAME?
		bool								m_success;
		wxString							m_parse_error;

		// the actual data
		std::optional<bool>					m_paused;
		std::optional<bool>					m_polling_input_seq;
		std::optional<wxString>				m_startup_text;
		std::optional<wxString>				m_frameskip;
		std::optional<wxString>				m_speed_text;
		std::optional<bool>					m_throttled;
		std::optional<float>				m_throttle_rate;
		std::optional<int>					m_sound_attenuation;
		std::optional<std::vector<image>>	m_images;
		std::optional<std::vector<input>>	m_inputs;

		static update read(wxTextInputStream &input);
	};
};

#endif // STATUS_H
