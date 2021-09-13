/***************************************************************************

    info.h

    Representation of MAME info, outputted by -listxml

***************************************************************************/

#pragma once

#ifndef INFO_H
#define INFO_H

#include <array>
#include <vector>
#include <unordered_map>
#include <iterator>

#include "bindata.h"
#include "utility.h"


//**************************************************************************
//  BINARY REPRESENTATIONS
//**************************************************************************

namespace info
{
	namespace binaries
	{
		const std::uint64_t MAGIC_HDR = 0x4D414D45494E464F;		// MAMEINFO
		const std::uint16_t MAGIC_STRINGTABLE_BEGIN = 0x9D9B;
		const std::uint16_t MAGIC_STRINGTABLE_END = 0x9F99;

		struct header
		{
			std::uint64_t	m_magic;
			std::uint64_t	m_sizes_hash;
			std::uint32_t	m_build_strindex;
			std::uint32_t	m_machines_count;
			std::uint32_t	m_biossets_count;
			std::uint32_t	m_roms_count;
			std::uint32_t	m_disks_count;
			std::uint32_t	m_devices_count;
			std::uint32_t	m_slots_count;
			std::uint32_t	m_slot_options_count;
			std::uint32_t	m_features_count;
			std::uint32_t	m_chips_count;
			std::uint32_t	m_displays_count;
			std::uint32_t	m_samples_count;
			std::uint32_t	m_configurations_count;
			std::uint32_t	m_configuration_settings_count;
			std::uint32_t	m_configuration_conditions_count;
			std::uint32_t	m_software_lists_count;
			std::uint32_t	m_ram_options_count;
		};

		struct machine
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_sourcefile_strindex;
			std::uint32_t	m_clone_of_machindex;
			std::uint32_t	m_rom_of_machindex;
			std::uint32_t	m_description_strindex;
			std::uint32_t	m_year_strindex;
			std::uint32_t	m_manufacturer_strindex;
			std::uint32_t	m_biossets_index;
			std::uint32_t	m_biossets_count;
			std::uint32_t	m_roms_index;
			std::uint32_t	m_roms_count;
			std::uint32_t	m_disks_index;
			std::uint32_t	m_disks_count;
			std::uint32_t	m_features_index;
			std::uint32_t	m_features_count;
			std::uint32_t	m_chips_index;
			std::uint32_t	m_chips_count;
			std::uint32_t	m_displays_index;
			std::uint32_t	m_displays_count;
			std::uint32_t	m_samples_index;
			std::uint32_t	m_samples_count;
			std::uint32_t	m_configurations_index;
			std::uint32_t	m_configurations_count;
			std::uint32_t	m_software_lists_index;
			std::uint32_t	m_software_lists_count;
			std::uint32_t	m_ram_options_index;
			std::uint32_t	m_ram_options_count;
			std::uint32_t	m_devices_index;
			std::uint32_t	m_devices_count;
			std::uint32_t	m_slots_index;
			std::uint32_t	m_slots_count;
			std::uint8_t	m_runnable;
			std::uint8_t	m_is_bios;
			std::uint8_t	m_is_device;
			std::uint8_t	m_is_mechanical;
			std::uint8_t	m_quality_status;
			std::uint8_t	m_quality_emulation;
			std::uint8_t	m_quality_cocktail;
			std::uint8_t	m_save_state_supported;
			std::uint8_t	m_unofficial;
			std::uint8_t	m_incomplete;
			std::uint8_t	m_sound_channels;
		};

		struct biosset
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_description_strindex;
			std::uint8_t	m_default;
		};

		struct rom
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_bios_strindex;
			std::uint32_t	m_size;
			std::uint8_t	m_crc32[4];
			std::uint8_t	m_sha1[20];
			std::uint32_t	m_merge_strindex;
			std::uint32_t	m_region_strindex;
			std::uint32_t	m_offset;
			std::uint8_t	m_status;
			std::uint8_t	m_optional;
		};

		struct disk
		{
			std::uint32_t	m_name_strindex;
			std::uint8_t	m_sha1[20];
			std::uint32_t	m_merge_strindex;
			std::uint32_t	m_region_strindex;
			std::uint32_t	m_index;
			std::uint8_t	m_writable;
			std::uint8_t	m_status;
			std::uint8_t	m_optional;
		};

		struct feature
		{
			std::uint8_t	m_type;
			std::uint8_t	m_status;
			std::uint8_t	m_overall;
		};

		struct chip
		{
			std::uint64_t	m_clock;
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_name_strindex;
			std::uint8_t	m_type;
		};

		struct display
		{
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_width;
			std::uint32_t	m_height;
			float			m_refresh;
			std::uint64_t	m_pixclock;
			std::uint32_t	m_htotal;
			std::uint32_t	m_hbend;
			std::uint32_t	m_hbstart;
			std::uint32_t	m_vtotal;
			std::uint32_t	m_vbend;
			std::uint32_t	m_vbstart;
			std::uint8_t	m_type;
			std::uint8_t	m_rotate;
			std::uint8_t	m_flipx;
		};

		struct sample
		{
			std::uint32_t	m_name_strindex;
		};

		struct configuration
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_mask;
			std::uint32_t	m_configuration_settings_index;
			std::uint32_t	m_configuration_settings_count;
		};

		struct configuration_condition
		{
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_mask;
			std::uint32_t	m_value;
			std::uint8_t	m_relation;
		};

		struct configuration_setting
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_value;
			std::uint32_t	m_conditions_index;
		};

		struct device
		{
			std::uint32_t	m_type_strindex;
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_interface_strindex;
			std::uint32_t	m_instance_name_strindex;
			std::uint32_t	m_extensions_strindex;
			std::uint8_t	m_mandatory;
		};

		struct slot
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_slot_options_index;
			std::uint32_t	m_slot_options_count;
		};

		struct slot_option
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_devname_strindex;
			std::uint8_t	m_is_default;
		};

		struct software_list
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_filter_strindex;
			std::uint8_t	m_status;
		};

		struct ram_option
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_value;
			std::uint8_t	m_is_default;
		};

		class salt
		{
		public:
			salt() : m_magic1(3133731337), m_magic2(0xF00D), m_version(1) { }

		private:
			std::uint32_t	m_magic1;
			std::uint16_t	m_magic2;
			std::uint16_t	m_version;
		};
	};
};


//**************************************************************************
//  INFO CLASSES
//**************************************************************************

namespace info
{
	class database;
	class database_builder;
	class machine;

	// ======================> biosset
	class biosset : public bindata::entry<database, biosset, binaries::biosset>
	{
	public:
		biosset(const database &db, const binaries::biosset &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		const QString &description() const { return get_string(inner().m_description_strindex); }
		bool is_default() const { return inner().m_default != 0; }
	};


	// ======================> rom
	class rom : public bindata::entry<database, rom, binaries::rom>
	{
	public:
		enum class dump_status_t
		{
			UNKNOWN,
			BADDUMP,
			NODUMP,
			GOOD
		};

		rom(const database &db, const binaries::rom &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		const QString &bios() const { return get_string(inner().m_bios_strindex); }
		std::uint32_t size() const { return inner().m_size; }
		std::uint32_t crc32() const
		{
			// doing this to avoid a silly Info DB breakage
			return ((std::uint32_t)inner().m_crc32[0]) << 24
				| ((std::uint32_t)inner().m_crc32[1]) << 16
				| ((std::uint32_t)inner().m_crc32[2]) << 8
				| ((std::uint32_t)inner().m_crc32[3]) << 0;
		}
		std::array<uint8_t, 20> sha1() const { return std::to_array(inner().m_sha1); }
		const QString &merge() const { return get_string(inner().m_merge_strindex); }
		const QString &region() const { return get_string(inner().m_region_strindex); }
		std::uint32_t offset() const { return inner().m_offset; }
		dump_status_t status() const { return (dump_status_t)inner().m_status; }
		bool optional() const { return inner().m_optional != 0; }
	};


	// ======================> disk
	class disk : public bindata::entry<database, disk, binaries::disk>
	{
	public:
		typedef rom::dump_status_t dump_status_t;

		disk(const database &db, const binaries::disk &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		std::array<uint8_t, 20> sha1() const { return std::to_array(inner().m_sha1); }
		const QString &merge() const { return get_string(inner().m_merge_strindex); }
		const QString &region() const { return get_string(inner().m_region_strindex); }
		std::uint32_t index() const { return inner().m_index; }
		bool writable() const { return inner().m_writable != 0; }
		dump_status_t status() const { return (dump_status_t)inner().m_status; }
		bool optional() const { return inner().m_optional != 0; }
	};


	// ======================> device
	class device : public bindata::entry<database, device, binaries::device>
	{
	public:
		device(const database &db, const binaries::device &inner)
			: entry(db, inner)
		{
		}

		const QString &type() const { return get_string(inner().m_type_strindex); }
		const QString &tag() const { return get_string(inner().m_tag_strindex); }
		const QString &devinterface() const { return get_string(inner().m_interface_strindex); }
		const QString &instance_name() const { return get_string(inner().m_instance_name_strindex); }
		const QString &extensions() const { return get_string(inner().m_extensions_strindex); }
		bool mandatory() const { return inner().m_mandatory != 0; }
	};


	// ======================> slot_option
	class slot_option : public bindata::entry<database, slot_option, binaries::slot_option>
	{
	public:
		slot_option(const database &db, const binaries::slot_option &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		const QString &devname() const { return get_string(inner().m_devname_strindex); }
		bool is_default() const { return inner().m_is_default; }
		std::optional<info::machine> machine() const;
	};


	// ======================> slot
	class slot : public bindata::entry<database, slot, binaries::slot>
	{
	public:
		slot(const database &db, const binaries::slot &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		slot_option::view options() const;
	};


	// ======================> configuration_setting
	class configuration_setting : public bindata::entry<database, configuration_setting, binaries::configuration_setting>
	{
	public:
		configuration_setting(const database &db, const binaries::configuration_setting &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		std::uint32_t value() const { return inner().m_value; }
	};


	// ======================> configuration_condition
	class configuration_condition : public bindata::entry<database, configuration_condition, binaries::configuration_condition>
	{
	public:
		enum class relation_t
		{
			UNKNOWN,
			EQ,
			NE,
			GT,
			LE,
			LT,
			GE
		};

		configuration_condition(const database &db, const binaries::configuration_condition &inner)
			: entry(db, inner)
		{
		}

		const QString &tag() const { return get_string(inner().m_tag_strindex); }
		std::uint32_t mask() const { return inner().m_mask; }
		std::uint32_t value() const { return inner().m_value; }
		relation_t relation() const { return static_cast<relation_t>(inner().m_relation); }
	};


	// ======================> chip
	class chip : public bindata::entry<database, chip, binaries::chip>
	{
	public:
		enum class type_t
		{
			UNKNOWN,
			CPU,
			AUDIO
		};

		chip(const database &db, const binaries::chip &inner)
			: entry(db, inner)
		{
		}

		type_t type() const			{ return (type_t) inner().m_type; }
		const QString &name() const	{ return get_string(inner().m_name_strindex); }
		const QString &tag() const	{ return get_string(inner().m_tag_strindex); }
		std::uint64_t clock() const	{ return inner().m_clock; }
	};


	// ======================> display
	class display : public bindata::entry<database, display, binaries::display>
	{
	public:
		enum class type_t
		{
			UNKNOWN,
			RASTER,
			VECTOR,
			LCD,
			SVG
		};

		enum class rotation_t
		{
			UNKNOWN,
			ROT0,
			ROT90,
			ROT180,
			ROT270
		};

		display(const database &db, const binaries::display &inner)
			: entry(db, inner)
		{
		}

		type_t type() const { return (type_t)inner().m_type; }
	};


	// ======================> sample
	class sample : public bindata::entry<database, sample, binaries::sample>
	{
	public:
		sample(const database &db, const binaries::sample &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
	};


	// ======================> configuration
	class configuration : public bindata::entry<database, configuration, binaries::configuration>
	{
	public:
		configuration(const database &db, const binaries::configuration &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		const QString &tag() const { return get_string(inner().m_tag_strindex); }
		std::uint32_t mask() const { return inner().m_mask; }
		configuration_setting::view settings() const;
	};


	// ======================> software_list
	class software_list : public bindata::entry<database, software_list, binaries::software_list>
	{
	public:
		enum class status_type
		{
			ORIGINAL,
			COMPATIBLE
		};

		software_list(const database &db, const binaries::software_list &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		const QString &filter() const { return get_string(inner().m_filter_strindex); }
		status_type status() const { return static_cast<status_type>(inner().m_status); }
	};


	// ======================> ram_option
	class ram_option : public bindata::entry<database, ram_option, binaries::ram_option>
	{
	public:
		enum class status_type
		{
			ORIGINAL,
			COMPATIBLE
		};

		ram_option(const database &db, const binaries::ram_option &inner)
			: entry(db, inner)
		{
		}

		const QString &name() const { return get_string(inner().m_name_strindex); }
		std::uint32_t value() const { return inner().m_value; }
		bool is_default() const { return inner().m_is_default; }
	};


	// ======================> feature
	class feature : public bindata::entry<database, feature, binaries::feature>
	{
	public:
		enum class type_t
		{
			UNKNOWN,
			PROTECTION,
			TIMING,
			GRAPHICS,
			PALETTE,
			SOUND,
			CAPTURE,
			CAMERA,
			MICROPHONE,
			CONTROLS,
			KEYBOARD,
			MOUSE,
			MEDIA,
			DISK,
			PRINTER,
			TAPE,
			PUNCH,
			DRUM,
			ROM,
			COMMS,
			LAN,
			WAN,
			COUNT
		};

		enum class quality_t
		{
			UNKNOWN,
			UNEMULATED,
			IMPERFECT
		};

		feature(const database &db, const binaries::feature &inner)
			: entry(db, inner)
		{
		}

		// properties
		type_t type() const { return (type_t) inner().m_type; }
		quality_t status() const { return (quality_t) inner().m_status; }
		quality_t overall() const { return (quality_t) inner().m_overall; }
	};

	// ======================> machine
	class machine : public bindata::entry<database, machine, binaries::machine>
	{
	public:
		enum class driver_quality_t
		{
			UNKNOWN,
			GOOD,
			IMPERFECT,
			PRELIMINARY
		};

		machine(const database &db, const binaries::machine &inner)
			: entry(db, inner)
		{
		}

		// methods
		std::optional<info::device> find_device(const QString &tag) const;
		std::optional<info::chip> find_chip(const QString &chipName) const;
		std::optional<info::machine> clone_of() const;
		std::optional<info::machine> rom_of() const;

		// properties
		bool runnable() const								{ return inner().m_runnable; }
		std::optional<bool> is_bios() const					{ return decode_optional_bool(inner().m_is_bios); }
		std::optional<bool> is_device() const				{ return decode_optional_bool(inner().m_is_device); }
		std::optional<bool> is_mechanical() const			{ return decode_optional_bool(inner().m_is_mechanical); }
		std::optional<bool> unofficial() const				{ return decode_optional_bool(inner().m_unofficial); }
		std::optional<bool> save_state_supported() const	{ return decode_optional_bool(inner().m_save_state_supported); }
		std::optional<int> sound_channels() const			{ return inner().m_sound_channels != ~0 ? inner().m_sound_channels : std::optional<int>(); }
		const QString &name() const							{ return get_string(inner().m_name_strindex); }
		const QString &sourcefile() const					{ return get_string(inner().m_sourcefile_strindex); }
		const QString &description() const					{ return get_string(inner().m_description_strindex); }
		const QString &year() const							{ return get_string(inner().m_year_strindex); }
		const QString &manufacturer() const					{ return get_string(inner().m_manufacturer_strindex); }

		// operators
		bool operator==(const info::machine &that) const
		{
			return name() == that.name();
		}

		// views
		biosset::view				biossets() const;
		rom::view					roms() const;
		disk::view					disks() const;
		device::view 				devices() const;
		slot::view					devslots() const;
		chip::view					chips() const;
		display::view				displays() const;
		sample::view				samples() const;
		configuration::view			configurations() const;
		software_list::view			software_lists() const;
		ram_option::view			ram_options() const;

	private:
		static std::optional<bool> decode_optional_bool(std::uint8_t b)
		{
			return b <= 1
				? std::optional<bool>(b)
				: std::nullopt;
		}
	};


	// ======================> database
	class database
	{
		template<typename TDatabase, typename TPublic, typename TBinary>
		friend class ::bindata::view;
		friend class database_builder;
	public:
		database()
			: m_version(&util::g_empty_string)
		{
		}

		// publically usable functions
		bool load(const QString &file_name, const QString &expected_version = "");
		bool load(QIODevice &input, const QString &expected_version = "");
		bool load(const QByteArray &byteArray, const QString &expected_version = "");
		void reset();
		std::optional<machine> find_machine(const QString &machine_name) const;
		const QString &version() const			{ return *m_version; }
		void addOnChangedHandler(std::function<void()> &&onChanged);

		// views
		auto machines() const					{ return machine::view(*this, m_state.m_machines_position); }
		auto biossets() const					{ return biosset::view(*this, m_state.m_biossets_position); }
		auto roms() const						{ return rom::view(*this, m_state.m_roms_position); }
		auto disks() const						{ return disk::view(*this, m_state.m_disks_position); }
		auto devices() const					{ return device::view(*this, m_state.m_devices_position); }
		auto devslots() const					{ return slot::view(*this, m_state.m_slots_position); }
		auto slot_options() const				{ return slot_option::view(*this, m_state.m_slot_options_position); }
		auto chips() const						{ return chip::view(*this, m_state.m_chips_position); }
		auto displays() const					{ return display::view(*this, m_state.m_displays_position); }
		auto samples() const					{ return sample::view(*this, m_state.m_samples_position); }
		auto configurations() const				{ return configuration::view(*this, m_state.m_configurations_position); }
		auto configuration_settings() const		{ return configuration_setting::view(*this, m_state.m_configuration_settings_position); }
		auto configuration_conditions() const	{ return configuration_condition::view(*this, m_state.m_configuration_conditions_position); }
		auto software_lists() const				{ return software_list::view(*this, m_state.m_software_lists_position); }
		auto ram_options() const				{ return ram_option::view(*this, m_state.m_ram_options_position); }

		// statics
		static uint64_t calculate_sizes_hash();

		// should only be called by info classes
		const QString &get_string(std::uint32_t offset) const;

	private:
		struct State
		{
			State();

			std::vector<std::uint8_t>						m_data;
			bindata::view_position							m_machines_position;
			bindata::view_position							m_biossets_position;
			bindata::view_position							m_roms_position;
			bindata::view_position							m_disks_position;
			bindata::view_position							m_devices_position;
			bindata::view_position							m_slots_position;
			bindata::view_position							m_slot_options_position;
			bindata::view_position							m_features_position;
			bindata::view_position							m_chips_position;
			bindata::view_position							m_displays_position;
			bindata::view_position							m_samples_position;
			bindata::view_position							m_configurations_position;
			bindata::view_position							m_configuration_settings_position;
			bindata::view_position							m_configuration_conditions_position;
			bindata::view_position							m_software_lists_position;
			bindata::view_position							m_ram_options_position;
			std::uint32_t									m_string_table_offset;
		};

		// member variables
		State												m_state;
		mutable std::unordered_map<std::uint32_t, QString>	m_loaded_strings;
		const QString *										m_version;
		std::vector<std::function<void()>>					m_onChangedHandlers;

		// data access
		template<typename T>
		void getDataSpan(std::span<const T> &span, size_t offset, std::optional<size_t> count = { }) const
		{
			span = getDataSpan<T>(offset, count);
		}

		// data access
		template<typename T>
		std::span<const T> getDataSpan(size_t offset, std::optional<size_t> count = { }) const
		{
			std::optional<std::span<const T>> span = tryGetDataSpan<T>(m_state, offset, count);
			if (!span)
				throw false;
			return *span;
		}

		// data access
		template<typename T>
		static std::optional<std::span<const T>> tryGetDataSpan(const State &state, size_t offset, std::optional<size_t> count = { })
		{
			size_t actualCount = count.value_or(state.m_data.size() - offset);
			if (offset > state.m_data.size() || (offset + actualCount * sizeof(T) > state.m_data.size()))
				return { };
			const T *ptr = reinterpret_cast<const T *>(&state.m_data.data()[offset]);
			return std::span(ptr, ptr + actualCount);
		}

		// private functions
		void onChanged();
		static std::optional<std::uint32_t> tryEncodeSmallStringChar(char8_t ch);
		static std::optional<std::uint32_t> tryEncodeAsSmallString(std::u8string_view s);
		static std::optional<std::array<char8_t, 6>> tryDecodeAsSmallString(std::uint32_t value);
	};

	inline biosset::view				machine::biossets() const		{ return db().biossets().subview(inner().m_biossets_index, inner().m_biossets_count); }
	inline rom::view					machine::roms() const			{ return db().roms().subview(inner().m_roms_index, inner().m_roms_count); }
	inline disk::view					machine::disks() const			{ return db().disks().subview(inner().m_disks_index, inner().m_disks_count); }
	inline device::view					machine::devices() const		{ return db().devices().subview(inner().m_devices_index, inner().m_devices_count); }
	inline slot::view					machine::devslots() const		{ return db().devslots().subview(inner().m_slots_index, inner().m_slots_count); }
	inline slot_option::view			slot::options() const			{ return db().slot_options().subview(inner().m_slot_options_index, inner().m_slot_options_count); }
	inline chip::view					machine::chips() const			{ return db().chips().subview(inner().m_chips_index, inner().m_chips_count); }
	inline display::view				machine::displays() const		{ return db().displays().subview(inner().m_displays_index, inner().m_displays_count); }
	inline sample::view					machine::samples() const		{ return db().samples().subview(inner().m_samples_index, inner().m_samples_count); }
	inline configuration::view			machine::configurations() const	{ return db().configurations().subview(inner().m_configurations_index, inner().m_configurations_count); }
	inline configuration_setting::view	configuration::settings() const	{ return db().configuration_settings().subview(inner().m_configuration_settings_index, inner().m_configuration_settings_count); }
	inline software_list::view			machine::software_lists() const	{ return db().software_lists().subview(inner().m_software_lists_index, inner().m_software_lists_count); }
	inline ram_option::view				machine::ram_options() const	{ return db().ram_options().subview(inner().m_ram_options_index, inner().m_ram_options_count); }
};


#endif // INFO_H

