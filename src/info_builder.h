/***************************************************************************

	info_builder.h

	Code to build MAME info DB

***************************************************************************/

#pragma once

#ifndef INFO_BUILDER_H
#define INFO_BUILDER_H

// bletchmame headers
#include "info.h"
#include "xmlparser.h"

// standard headers
#include <forward_list>

class QDataStream;

namespace info
{
	// ======================> database_builder
	class database_builder
	{
	public:
		class Test;

		typedef std::function<void(int machineCount, std::u8string_view machineName, std::u8string_view machineDescription)> ProcessXmlCallback;

		// ctors
		database_builder() = default;
		database_builder(const database_builder &) = delete;
		database_builder(database_builder &&) = default;

		// methods
		bool process_xml(QIODevice &stream, QString &error_message, const ProcessXmlCallback &progressCallback = { });
		void emit_info(QIODevice &stream) const;

	private:
		// ======================> string_table
		class string_table
		{
		public:
			typedef std::array<char8_t, 6> SsoBuffer;

			string_table();
			void shrinkToFit();
			std::uint32_t get(std::u8string_view string);
			std::uint32_t get(const XmlParser::Attributes &attributes, const char *attribute);
			std::span<const char8_t> data() const;
			const char8_t *lookup(std::uint32_t value, SsoBuffer &ssoBuffer) const;
			template<typename T> void embed_value(T value);

		private:
			typedef std::forward_list<std::uint32_t> MapBucket;

			std::vector<char8_t>								m_data;
			std::unique_ptr<std::array<MapBucket, 318677>>		m_mapBuckets;
		};

		info::binaries::header									m_salted_header;
		std::vector<info::binaries::machine>					m_machines;
		std::vector<info::binaries::biosset>					m_biossets;
		std::vector<info::binaries::rom>						m_roms;
		std::vector<info::binaries::disk>						m_disks;
		std::vector<info::binaries::device>						m_devices;
		std::vector<info::binaries::slot>						m_slots;
		std::vector<info::binaries::slot_option>				m_slot_options;
		std::vector<info::binaries::feature>					m_features;
		std::vector<info::binaries::chip>						m_chips;
		std::vector<info::binaries::display>					m_displays;
		std::vector<info::binaries::sample>						m_samples;
		std::vector<info::binaries::configuration>				m_configurations;
		std::vector<info::binaries::configuration_condition>	m_configuration_conditions;
		std::vector<info::binaries::configuration_setting>		m_configuration_settings;
		std::vector<info::binaries::software_list>				m_software_lists;
		std::vector<info::binaries::ram_option>					m_ram_options;
		string_table											m_strings;
	};
}


#endif // INFO_BUILDER_H
