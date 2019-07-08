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
#include <wx/string.h>


//**************************************************************************
//  BINARY REPRESENTATIONS
//**************************************************************************

namespace info
{
	namespace binaries
	{
		const std::uint32_t MAGIC_HEADER = 3133731337;
		const std::uint16_t MAGIC_STRINGTABLE_BEGIN = 0x9D9B;
		const std::uint16_t MAGIC_STRINGTABLE_END = 0x9F99;
		const std::uint32_t VERSION = 2;

		struct header
		{
			std::uint32_t	m_magic;
			std::uint32_t	m_version;
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
			std::uint32_t	m_configuration_index;
			std::uint32_t	m_tag_strindex;
			std::uint32_t	m_mask;
			std::uint32_t	m_relation_strindex;
			std::uint32_t	m_value;
		};

		struct configuration_setting
		{
			std::uint32_t	m_configuration_index;
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
	};
};


//**************************************************************************
//  INFO CLASSES
//**************************************************************************

namespace info
{
	class database;

	template<typename TBinary>
	class entry_base
	{
	protected:
		entry_base(const database &db, const TBinary &inner)
			: m_db(db)
			, m_inner(inner)
		{
		}

		const database &db() const		{ return m_db; }
		const TBinary &inner() const	{ return m_inner; }
		const wxString &get_string(std::uint32_t strindex) const;

	private:
		const database &	m_db;
		const TBinary &		m_inner;
	};

	template<typename TPublic, typename TBinary>
	class view
	{
	public:
		view() : m_db(nullptr), m_offset(0), m_count(0) { }
		view(const database &db, size_t offset, std::uint32_t count) : m_db(&db), m_offset(offset), m_count(count) { }
		view(const view &that) = default;
		view(view &&that) = default;

		view &operator=(const view &that)
		{
			m_db = that.m_db;
			m_offset = that.m_offset;
			m_count = that.m_count;
			return *this;
		}

		bool operator==(const view &that) const
		{
			return m_db == that.m_db
				&& m_offset == that.m_offset
				&& m_count == that.m_count;
		}

		TPublic operator[](std::uint32_t position) const;
		size_t size() const { return m_count; }

		view subview(std::uint32_t index, std::uint32_t count) const
		{
			if (index >= m_count || (index + count > m_count))
				throw false;
			return count > 0
				? view(*m_db, m_offset + index * sizeof(TBinary), count)
				: view();
		}

		class iterator
		{
		public:
			iterator(const view &view, uint32_t position)
				: m_view(view)
				, m_position(position)
			{
			}

			using iterator_category = std::random_access_iterator_tag;
			using value_type = TPublic;
			using difference_type = ssize_t;
			using pointer = TPublic * ;
			using reference = TPublic & ;

			TPublic operator*() const { return m_view[m_position]; }
			TPublic operator->() const { return m_view[m_position]; }

			bool operator<(const iterator &that)
			{
				asset_compatible_iterator(that);
				return m_position < that.m_position;
			}

			bool operator!=(const iterator &that)
			{
				asset_compatible_iterator(that);
				return m_position != that.m_position;
			}

			iterator &operator=(const iterator &that)
			{
				asset_compatible_iterator(that);
				m_position = that.m_position;
				return *this;
			}

			void operator++()
			{
				m_position++;
			}

			ssize_t operator-(const iterator &that)
			{
				asset_compatible_iterator(that);
				return ((ssize_t)m_position) - ((ssize_t)that.m_position);
			}

		private:
			view				m_view;
			uint32_t			m_position;

			void asset_compatible_iterator(const iterator &that)
			{
				assert(m_view == that.m_view);
				(void)that;
			}
		};

		typedef iterator const_iterator;

		iterator begin() const { return iterator(*this, 0); }
		iterator end() const { return iterator(*this, m_count); }
		const_iterator cbegin() const { return begin(); }
		const_iterator cend() const { return end(); }

	private:
		const database *	m_db;
		size_t				m_offset;
		uint32_t			m_count;
	};

	class device : public entry_base<binaries::device>
	{
	public:
		device(const database &db, const binaries::device &inner)
			: entry_base(db, inner)
		{
		}

		const wxString &type() const { return get_string(inner().m_type_strindex); }
		const wxString &tag() const { return get_string(inner().m_tag_strindex); }
		const wxString &instance_name() const { return get_string(inner().m_instance_name_strindex); }
		const wxString &extensions() const { return get_string(inner().m_extensions_strindex); }
		bool mandatory() const { return inner().m_mandatory != 0; }
	};

	class configuration_setting : public entry_base<binaries::configuration_setting>
	{
	public:
		configuration_setting(const database &db, const binaries::configuration_setting &inner)
			: entry_base(db, inner)
		{
		}

		const wxString &name() const { return get_string(inner().m_name_strindex); }
		std::uint32_t value() const { return inner().m_value; }
	};

	class configuration : public entry_base<binaries::configuration>
	{
	public:
		configuration(const database &db, const binaries::configuration &inner)
			: entry_base(db, inner)
		{
		}

		const wxString &name() const { return get_string(inner().m_name_strindex); }
		const wxString &tag() const { return get_string(inner().m_tag_strindex); }
		std::uint32_t mask() const { return inner().m_mask; }
		view<configuration_setting, binaries::configuration_setting> settings() const;
	};

	class machine : public entry_base<binaries::machine>
	{
	public:
		machine(const database &db, const binaries::machine &inner)
			: entry_base(db, inner)
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
		view<device, binaries::device>									devices() const;
		view<configuration, binaries::configuration>					configurations() const;
		view<configuration_setting, binaries::configuration_setting>	configuration_settings() const;
	};


	class database
	{
		template<typename TPublic, typename TBinary>
		friend class view;
	public:
		database()
			: m_machines_offset(0)
			, m_machines_count(0)
			, m_devices_offset(0)
			, m_devices_count(0)
			, m_configurations_offset(0)
			, m_configurations_count(0)
			, m_configuration_settings_offset(0)
			, m_configuration_settings_count(0)
			, m_string_table_offset(0)
			, m_version(&s_empty_string)
		{
		}

		// publically usable functions
		bool load(const wxString &file_name, const wxString &expected_version = wxT(""));
		void reset();
		const wxString &version() const			{ return *m_version; }
		void set_on_changed(std::function<void()> &&on_changed) { m_on_changed = std::move(on_changed); }

		// views
		auto machines() const				{ return view<machine, binaries::machine>(*this, m_machines_offset, m_machines_count); }
		auto devices() const				{ return view<device, binaries::device>(*this, m_devices_offset, m_devices_count); }
		auto configurations() const			{ return view<configuration, binaries::configuration>(*this, m_configurations_offset, m_configurations_count); }
		auto configuration_settings() const	{ return view<configuration_setting, binaries::configuration_setting>(*this, m_configuration_settings_offset, m_configuration_settings_count); }

		// should only be called by info classes
		const wxString &get_string(std::uint32_t offset) const;

	private:
		// member variables
		std::vector<std::uint8_t>							m_data;
		std::uint32_t										m_machines_offset;
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

		// statics
		static const wxString								s_empty_string;

		// private functions
		void on_changed();
		static std::vector<std::uint8_t> load_data(const wxString &file_name);
		static const char *get_string_from_data(const std::vector<std::uint8_t> &data, std::uint32_t string_table_offset, std::uint32_t offset);
	};

	template<typename TBinary> const wxString &entry_base<TBinary>::get_string(std::uint32_t strindex) const
	{
		return db().get_string(strindex);
	}

	template<typename TPublic, typename TBinary>
	inline TPublic view<TPublic, TBinary>::operator[](std::uint32_t position) const
	{
		if (position >= m_count)
			throw false;
		const std::uint8_t *ptr = &m_db->m_data.data()[m_offset + position * sizeof(TBinary)];
		return TPublic(*m_db, *reinterpret_cast<const TBinary *>(ptr));
	}


	inline view<device, binaries::device>				machine::devices() const						{ return db().devices().subview(inner().m_devices_index, inner().m_devices_count); }
	inline view<configuration, binaries::configuration>	machine::configurations() const					{ return db().configurations().subview(inner().m_configurations_index, inner().m_configurations_count); }
	inline view<configuration_setting, binaries::configuration_setting> configuration::settings() const	{ return db().configuration_settings().subview(inner().m_configuration_settings_index, inner().m_configuration_settings_count); }
};


#endif // INFO_H

