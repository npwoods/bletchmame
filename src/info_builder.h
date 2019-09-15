/***************************************************************************

	info_builder.h

	Code to build MAME info DB

***************************************************************************/

#pragma once

#ifndef INFO_BUILDER_H
#define INFO_BUILDER_H

#include <wx/stream.h>

#include "info.h"

namespace info
{
	class database_builder
	{
	public:
		bool process_xml(wxInputStream &input, wxString &error_message);
		void emit(wxOutputStream &stream);

	private:
		// ======================> string_table
		class string_table
		{
		public:
			string_table();
			std::uint32_t get(const std::string &string);
			std::uint32_t get(const wxString &string);
			const std::vector<char> &data() const;

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
		string_table											m_strings;
	};
};


#endif // INFO_BUILDER_H
