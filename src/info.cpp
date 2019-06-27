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

bool info::database::load(const wxString &file_name)
{
	// try to load the data
	if (!load_data(file_name))
		return false;

	// check the header
	const info::binaries::header &header(this->header());
	if (m_data.size() < sizeof(info::binaries::header))
		return false;
	if (header.m_magic != info::binaries::MAGIC_HEADER)
		return false;
	if (header.m_version != info::binaries::VERSION)
		return false;

	// offsets
	size_t machines_offset					= sizeof(info::binaries::header);
	size_t devices_offset					= machines_offset					+ (header.m_machines_count					* sizeof(info::binaries::machine));
	size_t configurations_offset			= devices_offset					+ (header.m_devices_count					* sizeof(info::binaries::device));
	size_t configuration_settings_offset	= configurations_offset				+ (header.m_configurations_count			* sizeof(info::binaries::configuration));
	size_t configuration_conditions_offset	= configuration_settings_offset		+ (header.m_configuration_settings_count	* sizeof(info::binaries::configuration_setting));
	size_t string_table_offset				= configuration_conditions_offset	+ (header.m_configuration_conditions_count	* sizeof(info::binaries::configuration_condition));

	// sanity check the string table
	if (m_data.size() < string_table_offset + 1)
		return false;
	if (m_data[string_table_offset] != '\0')
		return false;
	if (!unaligned_check(&m_data[string_table_offset + 1], info::binaries::MAGIC_STRINGTABLE_BEGIN))
		return false;
	if (m_data[m_data.size() - sizeof(info::binaries::MAGIC_STRINGTABLE_END) - 1] != '\0')
		return false;
	if (!unaligned_check(&m_data[m_data.size() - sizeof(info::binaries::MAGIC_STRINGTABLE_END)], info::binaries::MAGIC_STRINGTABLE_END))
		return false;

	// things look good, set up our tables
	m_view_machines	= decltype(m_view_machines)(*this,	machines_offset,	header.m_machines_count);
	m_view_devices	= decltype(m_view_devices)(*this,	devices_offset,		header.m_devices_count);

	// ...and set up string table info
	m_loaded_strings.clear();
	m_string_table_offset = string_table_offset;

	// ...and last but not least set up the version
	m_version = &get_string(header.m_build_strindex);
	return true;
}


//-------------------------------------------------
//  database::load_data
//-------------------------------------------------

bool info::database::load_data(const wxString &file_name)
{
	if (!wxFileExists(file_name))
		return false;

	wxFileInputStream input(file_name);
	if (!input.IsOk())
		return false;

	size_t size = input.GetSize();
	m_data.resize(size);
	return input.ReadAll(m_data.data(), size);
}


//-------------------------------------------------
//  database::header
//-------------------------------------------------

const info::binaries::header &info::database::header() const
{
	assert(m_data.size() >= sizeof(info::binaries::header));
	return *(reinterpret_cast<const info::binaries::header *>(m_data.data()));
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

	const char *string = reinterpret_cast<const char *>(&m_data.data()[m_string_table_offset + offset]);
	m_loaded_strings.emplace(offset, wxString::FromUTF8(string));
	return m_loaded_strings.find(offset)->second;
}
