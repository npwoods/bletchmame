/***************************************************************************

    info.cpp

    Representation of MAME info, outputted by -listxml

***************************************************************************/

#include <assert.h>
#include <QDataStream>

#include "info.h"
#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

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
//  load_data
//-------------------------------------------------

static std::vector<std::uint8_t> load_data(QDataStream &input, size_t size, info::binaries::header &header)
{
	// get the file size and read the header
	if (size <= sizeof(header))
		return {};
	if (input.readRawData((char *) &header, sizeof(header)) != sizeof(header))
		return {};

	// read the data
	std::vector<std::uint8_t> data;
	data.resize(size - sizeof(header));
	if (input.readRawData((char *) data.data(), (int)data.size()) != data.size())
		return {};

	// success! return it
	return data;
}


//-------------------------------------------------
//  get_string_from_data - needs to be separate so
//	we can do version check on "uncommitted" data
//-------------------------------------------------

static const char *get_string_from_data(const std::vector<std::uint8_t> &data, std::uint32_t string_table_offset, std::uint32_t offset)
{
	// sanity check
	if (offset >= data.size() || (string_table_offset + offset) >= data.size())
		return "";	// should not happen with a valid info DB

	// needs to be separate so we can call it on "uncommitted" data
	return reinterpret_cast<const char *>(&data.data()[string_table_offset + offset]);
}


//-------------------------------------------------
//  database::load
//-------------------------------------------------

bool info::database::load(const QString &file_name, const QString &expected_version)
{
	// check for file existance
	QFile file(file_name);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	// open up the file
	QDataStream input(&file);
	return load(input, file.bytesAvailable(), expected_version);
}


bool info::database::load(QDataStream &input, size_t size, const QString &expected_version)
{
	// try to load the data
	binaries::header salted_hdr;
	std::vector<std::uint8_t> data = load_data(input, size, salted_hdr);
	if (data.empty())
		return false;

	// unsalt the header
	binaries::header hdr = util::salt(salted_hdr, info::binaries::salt());

	// check the header
	if ((hdr.m_size_header != sizeof(binaries::header))
		|| (hdr.m_size_machine != sizeof(binaries::machine))
		|| (hdr.m_size_device != sizeof(binaries::device))
		|| (hdr.m_size_configuration != sizeof(binaries::configuration))
		|| (hdr.m_size_configuration_setting != sizeof(binaries::configuration_setting))
		|| (hdr.m_size_configuration_condition != sizeof(binaries::configuration_condition))
		|| (hdr.m_size_software_list != sizeof(binaries::software_list))
		|| (hdr.m_size_ram_option != sizeof(binaries::ram_option)))
	{
		return false;
	}

	// offsets
	size_t devices_offset					= 0									+ (hdr.m_machines_count					* sizeof(binaries::machine));
	size_t configurations_offset			= devices_offset					+ (hdr.m_devices_count					* sizeof(binaries::device));
	size_t configuration_settings_offset	= configurations_offset				+ (hdr.m_configurations_count			* sizeof(binaries::configuration));
	size_t configuration_conditions_offset	= configuration_settings_offset		+ (hdr.m_configuration_settings_count	* sizeof(binaries::configuration_setting));
	size_t software_lists_offset			= configuration_conditions_offset	+ (hdr.m_configuration_conditions_count * sizeof(binaries::configuration_condition));
	size_t ram_options_offset				= software_lists_offset				+ (hdr.m_software_lists_count			* sizeof(binaries::software_list));
	size_t string_table_offset				= ram_options_offset				+ (hdr.m_ram_options_count				* sizeof(binaries::ram_option));

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
	if (!expected_version.isEmpty() && expected_version != get_string_from_data(data, string_table_offset, hdr.m_build_strindex))
		return false;

	// finally things look good - first shrink the data array to drop the ending magic bytes
	data.resize(data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END));

	// ...move the data itself
	m_data = std::move(data);

	// ...and the tables
	m_machines_count = hdr.m_machines_count;
	m_devices_offset = devices_offset;
	m_devices_count = hdr.m_devices_count;
	m_configurations_offset = configurations_offset;
	m_configurations_count = hdr.m_configurations_count;
	m_configuration_settings_offset = configuration_settings_offset;
	m_configuration_settings_count = hdr.m_configuration_settings_count;
	m_configuration_conditions_offset = configuration_conditions_offset;
	m_configuration_conditions_count = hdr.m_configuration_conditions_count;
	m_software_lists_offset = software_lists_offset;
	m_software_lists_count = hdr.m_software_lists_count;
	m_ram_options_offset = ram_options_offset;
	m_ram_options_count = hdr.m_ram_options_count;

	// ...and set up string table info
	m_loaded_strings.clear();
	m_string_table_offset = string_table_offset;

	// ...and last but not least set up the version
	m_version = &get_string(hdr.m_build_strindex);

	// signal that we've changed and we're done
	on_changed();
	return true;
}


//-------------------------------------------------
//  database::reset
//-------------------------------------------------

void info::database::reset()
{
	m_data.resize(0);
	m_machines_count = 0;
	m_devices_offset = 0;
	m_devices_count = 0;
	m_configurations_offset = 0;
	m_configurations_count = 0;
	m_configuration_settings_offset = 0;
	m_configuration_settings_count = 0;
	m_configuration_conditions_offset = 0;
	m_configuration_conditions_count = 0;
	m_software_lists_offset = 0;
	m_software_lists_count = 0;
	m_ram_options_offset = 0;
	m_ram_options_count = 0;
	m_string_table_offset = 0;
	m_loaded_strings.clear();
	m_version = &util::g_empty_string;
	on_changed();
}


//-------------------------------------------------
//  database::on_changed
//-------------------------------------------------

void info::database::on_changed()
{
	if (m_on_changed)
		m_on_changed();
}


//-------------------------------------------------
//  database::get_string
//-------------------------------------------------

const QString &info::database::get_string(std::uint32_t offset) const
{
	if (m_string_table_offset + offset >= m_data.size())
		throw false;

	auto iter = m_loaded_strings.find(offset);
	if (iter != m_loaded_strings.end())
		return iter->second;

	const char *string = get_string_from_data(m_data, m_string_table_offset, offset);
	m_loaded_strings.emplace(offset, QString::fromUtf8(string));
	return m_loaded_strings.find(offset)->second;
}


//-------------------------------------------------
//  database::find_machine
//-------------------------------------------------

std::optional<info::machine> info::database::find_machine(const QString &machine_name) const
{
	auto iter = std::find_if(
		machines().begin(),
		machines().end(),
		[&machine_name](const info::machine m)
		{
			return m.name() == machine_name;
		});
	return iter != machines().end()
		? std::optional<info::machine>(*iter)
		: std::optional<info::machine>();
}
