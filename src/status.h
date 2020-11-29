/***************************************************************************

    status.h

    Tracking of MAME internal state updates

***************************************************************************/

#pragma once

#ifndef STATUS_H
#define STATUS_H

// pesky macro collisions
#ifdef min
#undef min
#endif

// pesky macro collisions
#ifdef max
#undef max
#endif

#include "observable/observable.hpp"
#include <QString>
#include <optional>

#include "utility.h"


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

class QDataStream;

typedef std::uint32_t ioport_value;

namespace status
{
	// ======================> image
	struct image
	{
		image() = default;
		image(const image &) = SHOULD_BE_DELETE;
		image(image &&that) = default;

		QString				m_tag;
		QString				m_instance_name;
		bool				m_is_readable;
		bool				m_is_writeable;
		bool				m_is_creatable;
		bool				m_must_be_loaded;
		QString				m_file_name;
		QString				m_display;

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
		QString				m_tokens;

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

		QString					m_port_tag;
		QString					m_name;
		ioport_value			m_mask;
		input_class				m_class;
		int						m_group;
		int						m_player;
		int						m_type;
		bool					m_is_analog;
		int						m_first_keyboard_code;
		ioport_value			m_value;
		std::vector<input_seq>	m_seqs;

		input &operator=(input &&that) = default;
		bool operator==(const input &that) const;
	};


	// ======================> input_device_class
	struct input_device_item
	{
		input_device_item() = default;
		input_device_item(const input_device_item &) = SHOULD_BE_DELETE;
		input_device_item(input_device_item &&that) = default;

		QString		m_name;
		QString		m_token;
		QString		m_code;

		input_device_item &operator=(input_device_item &&that) = default;
		bool operator==(const input_device_item &that) const;
	};


	// ======================> input_device
	struct input_device
	{
		input_device() = default;
		input_device(const input_device &) = SHOULD_BE_DELETE;
		input_device(input_device &&that) = default;

		QString							m_name;
		QString							m_id;
		int								m_index;
		std::vector<input_device_item>	m_items;
		
		input_device &operator=(input_device &&that) = default;
		bool operator==(const input_device &that) const;
	};


	// ======================> input_class
	struct input_class
	{
		input_class() = default;
		input_class(const input_class &) = SHOULD_BE_DELETE;
		input_class(input_class &&that) = default;

		QString							m_name;
		bool							m_enabled;
		bool							m_multi;
		std::vector<input_device>		m_devices;

		input_class &operator=(input_class &&that) = default;
		bool operator==(const input_class &that) const;
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


	// ======================> cheat_parameter
	struct cheat_parameter
	{
		cheat_parameter() = default;
		cheat_parameter(const cheat_parameter &) = SHOULD_BE_DELETE;
		cheat_parameter(cheat_parameter &&) = default;

		std::uint64_t						m_value;
		std::uint64_t						m_minimum;
		std::uint64_t						m_maximum;
		std::uint64_t						m_step;
		std::map<std::uint64_t, QString>	m_items;

		bool operator==(const cheat_parameter &that) const;
	};

	// ======================> cheat
	struct cheat
	{
		cheat() = default;
		cheat(const cheat &) = SHOULD_BE_DELETE;
		cheat(cheat &&) = default;

		QString							m_id;
		bool							m_enabled;
		bool							m_has_run_script;
		bool							m_has_on_script;
		bool							m_has_off_script;
		bool							m_has_change_script;
		QString							m_description;
		QString							m_comment;
		std::optional<cheat_parameter>	m_parameter;

		bool operator==(const cheat &that) const;
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
		QString							m_parse_error;

		// the actual data
		std::optional<machine_phase>				m_phase;
		std::optional<bool>							m_paused;
		std::optional<bool>							m_polling_input_seq;
		std::optional<bool>							m_has_input_using_mouse;
		std::optional<QString>						m_startup_text;
		std::optional<bool>							m_debugger_present;
		std::optional<float>						m_speed_percent;
		std::optional<QString>						m_frameskip;
		std::optional<int>							m_effective_frameskip;
		std::optional<bool>							m_throttled;
		std::optional<float>						m_throttle_rate;
		std::optional<bool>							m_is_recording;
		std::optional<int>							m_sound_attenuation;
		std::optional<std::vector<image>>			m_images;
		std::optional<std::vector<input>>			m_inputs;
		std::optional<std::vector<input_class>>		m_input_classes;
		std::optional<std::vector<cheat>>			m_cheats;

		static update read(QIODevice &input);

		update &operator=(update &&that) = default;
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
		observable::value<machine_phase> &				phase()						{ return m_phase; }
		observable::value<bool> &						paused()					{ return m_paused; }
		observable::value<bool> &						polling_input_seq()			{ return m_polling_input_seq; }
		observable::value<bool> &						has_input_using_mouse()		{ return m_has_input_using_mouse; }
		observable::value<QString> &					startup_text()				{ return m_startup_text; }
		observable::value<bool>	&						debugger_present()			{ return m_debugger_present; }
		observable::value<float> &						speed_percent()				{ return m_speed_percent; }
		observable::value<int> &						effective_frameskip()		{ return m_effective_frameskip; }
		observable::value<std::vector<image>> &			images()					{ return m_images; }
		observable::value<std::vector<input>> &			inputs()					{ return m_inputs; }
		observable::value<std::vector<input_class>> &	input_classes()				{ return m_input_classes; }
		observable::value<QString> &					frameskip()					{ return m_frameskip; }
		bool											throttled() const			{ return m_throttled; }
		observable::value<float> &						throttle_rate()				{ return m_throttle_rate; }
		bool											is_recording() const		{ return m_is_recording; }
		observable::value<int> &						sound_attenuation()			{ return m_sound_attenuation; }
		observable::value<std::vector<cheat>> &			cheats()					{ return m_cheats; }

		// higher level methods
		const image *find_image(const QString &tag) const;
		bool has_input_class(status::input::input_class input_class) const;

	private:
		observable::value<machine_phase>				m_phase;
		observable::value<bool>							m_paused;
		observable::value<bool>							m_polling_input_seq;
		observable::value<bool>							m_has_input_using_mouse;
		observable::value<QString>						m_startup_text;
		observable::value<bool>							m_debugger_present;
		observable::value<float>						m_speed_percent;
		observable::value<int>							m_effective_frameskip;
		observable::value<std::vector<image>>			m_images;
		observable::value<std::vector<input>>			m_inputs;
		observable::value<std::vector<input_class>>		m_input_classes;
		observable::value<QString>						m_frameskip;
		bool											m_throttled;
		observable::value<float>						m_throttle_rate;
		bool											m_is_recording;
		observable::value<int>							m_sound_attenuation;
		observable::value<bool>							m_cheats_enabled;
		observable::value<std::vector<cheat>>			m_cheats;

		template<typename TStateField, typename TUpdateField>
		bool take(TStateField &state_field, std::optional<TUpdateField> &update_field);
	};
};

#endif // STATUS_H
