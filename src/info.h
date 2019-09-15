/***************************************************************************

    info.h

    Representation of MAME info, outputted by -listxml

***************************************************************************/

#pragma once

#ifndef INFO_H
#define INFO_H

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
		const std::uint16_t MAGIC_STRINGTABLE_BEGIN = 0x9D9B;
		const std::uint16_t MAGIC_STRINGTABLE_END = 0x9F99;

		struct header
		{
			std::uint32_t	m_magic;
			std::uint32_t	m_version;
			std::uint8_t	m_size_header;
			std::uint8_t	m_size_machine;
			std::uint8_t	m_size_device;
			std::uint8_t	m_size_configuration;
			std::uint8_t	m_size_configuration_setting;
			std::uint8_t	m_size_configuration_condition;
			std::uint32_t	m_build_strindex;
			std::uint32_t	m_machines_count;
			std::uint32_t	m_devices_count;
			std::uint32_t	m_configurations_count;
			std::uint32_t	m_configuration_settings_count;
			std::uint32_t	m_configuration_conditions_count;
		};

		struct machine
		{
			std::uint32_t	m_name_strindex;
			std::uint32_t	m_sourcefile_strindex;
			std::uint32_t	m_clone_of_strindex;
			std::uint32_t	m_rom_of_strindex;
			std::uint32_t	m_description_strindex;
			std::uint32_t	m_year_strindex;
			std::uint32_t	m_manufacturer_strindex;
			std::uint32_t	m_configurations_index;
			std::uint32_t	m_configurations_count;
			std::uint32_t	m_devices_index;
			std::uint32_t	m_devices_count;
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
			std::uint32_t	m_relation_strindex;
			std::uint32_t	m_value;
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
			std::uint32_t	m_instance_name_strindex;
			std::uint32_t	m_extensions_strindex;
			std::uint8_t	m_mandatory;
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

	// ======================> device
	class device : public bindata::entry<database, device, binaries::device>
	{
	public:
		device(const database &db, const binaries::device &inner)
			: entry(db, inner)
		{
		}

		const wxString &type() const { return get_string(inner().m_type_strindex); }
		const wxString &tag() const { return get_string(inner().m_tag_strindex); }
		const wxString &instance_name() const { return get_string(inner().m_instance_name_strindex); }
		const wxString &extensions() const { return get_string(inner().m_extensions_strindex); }
		bool mandatory() const { return inner().m_mandatory != 0; }
	};


	// ======================> configuration_setting
	class configuration_setting : public bindata::entry<database, configuration_setting, binaries::configuration_setting>
	{
	public:
		configuration_setting(const database &db, const binaries::configuration_setting &inner)
			: entry(db, inner)
		{
		}

		const wxString &name() const { return get_string(inner().m_name_strindex); }
		std::uint32_t value() const { return inner().m_value; }
	};


	// ======================> configuration_setting
	class configuration : public bindata::entry<database, configuration, binaries::configuration>
	{
	public:
		configuration(const database &db, const binaries::configuration &inner)
			: entry(db, inner)
		{
		}

		const wxString &name() const { return get_string(inner().m_name_strindex); }
		const wxString &tag() const { return get_string(inner().m_tag_strindex); }
		std::uint32_t mask() const { return inner().m_mask; }
		configuration_setting::view settings() const;
	};


	// ======================> configuration_setting
	class machine : public bindata::entry<database, machine, binaries::machine>
	{
	public:
		machine(const database &db, const binaries::machine &inner)
			: entry(db, inner)
		{
		}

		// properties
		const wxString &name() const			{ return get_string(inner().m_name_strindex); }
		const wxString &sourcefile() const		{ return get_string(inner().m_sourcefile_strindex); }
		const wxString &clone_of() const		{ return get_string(inner().m_clone_of_strindex); }
		const wxString &rom_of() const			{ return get_string(inner().m_rom_of_strindex); }
		const wxString &description() const		{ return get_string(inner().m_description_strindex); }
		const wxString &year() const			{ return get_string(inner().m_year_strindex); }
		const wxString &manufacturer() const	{ return get_string(inner().m_manufacturer_strindex); }

		// views
		device::view 				devices() const;
		configuration::view			configurations() const;
	};


	// ======================> database
	class database
	{
		template<typename TDatabase, typename TPublic, typename TBinary>
		friend class ::bindata::view;
	public:
		database()
			: m_machines_count(0)
			, m_devices_offset(0)
			, m_devices_count(0)
			, m_configurations_offset(0)
			, m_configurations_count(0)
			, m_configuration_settings_offset(0)
			, m_configuration_settings_count(0)
			, m_string_table_offset(0)
			, m_version(&util::g_empty_string)
		{
		}

		// publically usable functions
		bool load(const wxString &file_name, const wxString &expected_version = wxT(""));
		bool load(wxInputStream &input, const wxString &expected_version = wxT(""));
		void reset();
		std::optional<machine> find_machine(const wxString &machine_name) const;
		const wxString &version() const			{ return *m_version; }
		void set_on_changed(std::function<void()> &&on_changed) { m_on_changed = std::move(on_changed); }

		// views
		auto machines() const				{ return machine::view(*this, 0, m_machines_count); }
		auto devices() const				{ return device::view(*this, m_devices_offset, m_devices_count); }
		auto configurations() const			{ return configuration::view(*this, m_configurations_offset, m_configurations_count); }
		auto configuration_settings() const	{ return configuration_setting::view(*this, m_configuration_settings_offset, m_configuration_settings_count); }

		// should only be called by info classes
		const wxString &get_string(std::uint32_t offset) const;

	private:
		// member variables
		std::vector<std::uint8_t>							m_data;
		std::uint32_t										m_machines_count;
		std::uint32_t										m_devices_offset;
		std::uint32_t										m_devices_count;
		std::uint32_t										m_configurations_offset;
		std::uint32_t										m_configurations_count;
		std::uint32_t										m_configuration_settings_offset;
		std::uint32_t										m_configuration_settings_count;
		size_t												m_string_table_offset;
		mutable std::unordered_map<std::uint32_t, wxString>	m_loaded_strings;
		const wxString *									m_version;
		std::function<void()>								m_on_changed;

		// private functions
		void on_changed();
	};

	inline device::view					machine::devices() const		{ return db().devices().subview(inner().m_devices_index, inner().m_devices_count); }
	inline configuration::view			machine::configurations() const	{ return db().configurations().subview(inner().m_configurations_index, inner().m_configurations_count); }
	inline configuration_setting::view	configuration::settings() const	{ return db().configuration_settings().subview(inner().m_configuration_settings_index, inner().m_configuration_settings_count); }
};


#endif // INFO_H

