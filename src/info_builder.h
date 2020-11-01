/***************************************************************************

	info_builder.h

	Code to build MAME info DB

***************************************************************************/

#pragma once

#ifndef INFO_BUILDER_H
#define INFO_BUILDER_H

class QDataStream;

#include "info.h"

namespace info
{
	// ======================> database_builder
	class database_builder
	{
	public:
		// ctors
		database_builder() = default;
		database_builder(const database_builder &) = delete;
		database_builder(database_builder &&) = default;

		// methods
		bool process_xml(QIODevice &stream, QString &error_message);
		bool process_xml(QDataStream &input, QString &error_message);
		void emit_info(QIODevice &stream) const;

	private:
		// ======================> string_table
		class string_table
		{
		public:
			string_table();
			std::uint32_t get(const std::string &string);
			std::uint32_t get(const QString &string);
			const std::vector<char> &data() const;
			const char *lookup(std::uint32_t value) const;

			template<typename T> void embed_value(T value)
			{
				const std::uint8_t *bytes = (const std::uint8_t *) & value;
				m_data.insert(m_data.end(), &bytes[0], &bytes[0] + sizeof(value));
			}

		private:
			std::vector<char>								m_data;
			std::unordered_map<std::string, std::uint32_t>	m_map;
		};

		info::binaries::header									m_salted_header;
		std::vector<info::binaries::machine>					m_machines;
		std::vector<info::binaries::device>						m_devices;
		std::vector<info::binaries::configuration>				m_configurations;
		std::vector<info::binaries::configuration_condition>	m_configuration_conditions;
		std::vector<info::binaries::configuration_setting>		m_configuration_settings;
		std::vector<info::binaries::software_list>				m_software_lists;
		std::vector<info::binaries::ram_option>					m_ram_options;
		string_table											m_strings;
	};
};


#endif // INFO_BUILDER_H
