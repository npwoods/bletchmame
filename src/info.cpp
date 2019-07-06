/***************************************************************************

    info.cpp

    Representation of MAME info, outputted by -listxml

***************************************************************************/

#include <assert.h>
#include <wx/wfstream.h>

#include "info.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const wxString info::database::s_empty_string;


//-------------------------------------------------
//  unaligned_check
//-------------------------------------------------

template<typename T>
static bool unaligned_check(const void *ptr, T value)
{
	T data;
	memcpy(&data, ptr, sizeof(data));
	return data == value;
}


//-------------------------------------------------
//  database::load
//-------------------------------------------------

bool info::database::load(const wxString &file_name, const wxString &expected_version)
{
	// try to load the data
	std::vector<std::uint8_t> data = load_data(file_name);
	if (data.empty())
		return false;

	// check the header
	if (data.size() < sizeof(binaries::header))
		return false;
	const binaries::header &hdr(*(reinterpret_cast<const info::binaries::header *>(data.data())));
	if (hdr.m_magic != binaries::MAGIC_HEADER)
		return false;
	if (hdr.m_version != binaries::VERSION)
		return false;

	// offsets
	size_t machines_offset					= sizeof(binaries::header);
	size_t devices_offset					= machines_offset					+ (hdr.m_machines_count					* sizeof(binaries::machine));
	size_t configurations_offset			= devices_offset					+ (hdr.m_devices_count					* sizeof(binaries::device));
	size_t configuration_settings_offset	= configurations_offset				+ (hdr.m_configurations_count			* sizeof(binaries::configuration));
	size_t configuration_conditions_offset	= configuration_settings_offset		+ (hdr.m_configuration_settings_count	* sizeof(binaries::configuration_setting));
	size_t string_table_offset				= configuration_conditions_offset	+ (hdr.m_configuration_conditions_count	* sizeof(binaries::configuration_condition));

	// sanity check the string table
	if (data.size() < string_table_offset + 1)
		return false;
	if (data[string_table_offset] != '\0')
		return false;
	if (!unaligned_check(&data[string_table_offset + 1], binaries::MAGIC_STRINGTABLE_BEGIN))
		return false;
	if (data[data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END) - 1] != '\0')
		return false;
	if (!unaligned_check(&data[data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END)], binaries::MAGIC_STRINGTABLE_END))
		return false;

	// version check if appropriate
	if (!expected_version.empty() && expected_version != get_string_from_data(data, string_table_offset, hdr.m_build_strindex))
		return false;

	// finally things look good, set up the data
	m_data = std::move(data);

	// ...and the tables
	m_machines_offset = machines_offset;
	m_machines_count = hdr.m_machines_count;
	m_devices_offset = devices_offset;
	m_devices_count = hdr.m_devices_count;

	// ...and set up string table info
	m_loaded_strings.clear();
	m_string_table_offset = string_table_offset;

	// ...and last but not least set up the version
	m_version = &get_string(hdr.m_build_strindex);
	return true;
}


//-------------------------------------------------
//  database::load_data
//-------------------------------------------------

std::vector<std::uint8_t> info::database::load_data(const wxString &file_name)
{
	if (!wxFileExists(file_name))
		return { };

	wxFileInputStream input(file_name);
	if (!input.IsOk())
		return { };

	size_t size = input.GetSize();
	if (size <= 0)
		return { };

	std::vector<std::uint8_t> data;
	data.resize(size);
	if (!input.ReadAll(data.data(), size))
		return { };

	return data;
}


//-------------------------------------------------
//  database::reset
//-------------------------------------------------

void info::database::reset()
{
	m_data.resize(0);
	m_machines_offset = 0;
	m_machines_count = 0;
	m_devices_offset = 0;
	m_devices_count = 0;
	m_string_table_offset = 0;
	m_loaded_strings.clear();
	m_version = nullptr;
}


//-------------------------------------------------
//  database::get_string
//-------------------------------------------------

const wxString &info::database::get_string(std::uint32_t offset) const
{
	if (m_string_table_offset + offset >= m_data.size())
		throw false;

	auto iter = m_loaded_strings.find(offset);
	if (iter != m_loaded_strings.end())
		return iter->second;

	const char *string = get_string_from_data(m_data, m_string_table_offset, offset);
	m_loaded_strings.emplace(offset, wxString::FromUTF8(string));
	return m_loaded_strings.find(offset)->second;
}


//-------------------------------------------------
//  database::get_string_from_data - needs to be
//	separate so we can do version check on
//	"uncommitted" data
//-------------------------------------------------

const char *info::database::get_string_from_data(const std::vector<std::uint8_t> &data, std::uint32_t string_table_offset, std::uint32_t offset)
{
	// sanity check
	size_t maximum_size = data.size() - sizeof(std::uint16_t);
	if (offset >= maximum_size || (string_table_offset + offset) >= maximum_size)
		return "";	// should not happen with a valid info DB

	// needs to be separate so we can call it on "uncommitted" data
	return reinterpret_cast<const char *>(&data.data()[string_table_offset + offset]);
}
