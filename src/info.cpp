/***************************************************************************

    info.cpp

    Representation of MAME info, outputted by -listxml

***************************************************************************/

#include <assert.h>
#include <stdexcept>

#include <QBuffer>
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

static std::vector<std::uint8_t> load_data(QIODevice &input, info::binaries::header &header)
{
	// get the file size and read the header
	size_t size = util::safe_static_cast<size_t>(input.bytesAvailable());
	if (size <= sizeof(header))
		return {};
	if (input.read((char *) &header, sizeof(header)) != sizeof(header))
		return {};

	// read the data
	std::vector<std::uint8_t> data;
	data.resize(size - sizeof(header));
	if (input.read((char *) data.data(), (int)data.size()) != data.size())
		return {};

	// success! return it
	return data;
}


//-------------------------------------------------
//  getPosition
//-------------------------------------------------

template<class T>
bindata::view_position getPosition(size_t &cursor, std::uint32_t count)
{
	std::uint32_t offset = util::safe_static_cast<std::uint32_t>(cursor);
	cursor += sizeof(T) * count;
	return bindata::view_position(offset, count);
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
	return load(file, expected_version);
}


//-------------------------------------------------
//  database::load - in practice this will likely
//	only be used by unit tests
//-------------------------------------------------

bool info::database::load(const QByteArray &byteArray, const QString &expected_version)
{
	QBuffer buffer((QByteArray *) &byteArray);
	return buffer.open(QIODevice::ReadOnly)
		&& load(buffer, expected_version);
}


//-------------------------------------------------
//  database::load
//-------------------------------------------------

bool info::database::load(QIODevice &input, const QString &expected_version)
{
	info::database::State newState;

	// try to load the data
	binaries::header salted_hdr;
	newState.m_data = load_data(input, salted_hdr);
	if (newState.m_data.empty())
		return false;

	// unsalt the header
	binaries::header hdr = util::salt(salted_hdr, info::binaries::salt());

	// check the header
	if ((hdr.m_magic != info::binaries::MAGIC_HDR) || (hdr.m_sizes_hash != calculate_sizes_hash()))
		return false;

	// positions
	size_t cursor = 0;
	newState.m_machines_position					= getPosition<binaries::machine>(cursor, hdr.m_machines_count);
	newState.m_devices_position						= getPosition<binaries::device>(cursor, hdr.m_devices_count);
	newState.m_slots_position						= getPosition<binaries::slot>(cursor, hdr.m_slots_count);
	newState.m_slot_options_position				= getPosition<binaries::slot>(cursor, hdr.m_slot_options_count);
	newState.m_configurations_position				= getPosition<binaries::configuration>(cursor, hdr.m_configurations_count);
	newState.m_configuration_settings_position		= getPosition<binaries::configuration_setting>(cursor, hdr.m_configuration_settings_count);
	newState.m_configuration_conditions_position	= getPosition<binaries::configuration_condition>(cursor, hdr.m_configuration_conditions_count);
	newState.m_software_lists_position				= getPosition<binaries::software_list>(cursor, hdr.m_software_lists_count);
	newState.m_ram_options_position					= getPosition<binaries::ram_option>(cursor, hdr.m_ram_options_count);
	newState.m_string_table_offset					= cursor;

	// sanity check the string table
	if (newState.m_data.size() < (size_t)newState.m_string_table_offset + 1)
		return false;
	if (newState.m_data[newState.m_string_table_offset] != '\0')
		return false;
	if (!unaligned_check(&newState.m_data[(size_t)newState.m_string_table_offset + 1], binaries::MAGIC_STRINGTABLE_BEGIN))
		return false;
	if (newState.m_data[newState.m_data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END) - 1] != '\0')
		return false;
	if (!unaligned_check(&newState.m_data[newState.m_data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END)], binaries::MAGIC_STRINGTABLE_END))
		return false;

	// version check if appropriate
	if (!expected_version.isEmpty() && expected_version != getStringFromData(newState, hdr.m_build_strindex))
		return false;

	// finally things look good - first shrink the data array to drop the ending magic bytes
	newState.m_data.resize(newState.m_data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END));

	// ...set the state
	m_state = std::move(newState);

	// ...and set up other incidental state
	m_loaded_strings.clear();
	m_version = &get_string(hdr.m_build_strindex);

	// signal that we've changed and we're done
	onChanged();
	return true;
}


//-------------------------------------------------
//  database::calculate_sizes_hash
//-------------------------------------------------
 
uint64_t info::database::calculate_sizes_hash()
{
	static const uint64_t sizes[] =
	{
		sizeof(info::binaries::header),
		sizeof(info::binaries::machine),
		sizeof(info::binaries::device),
		sizeof(info::binaries::slot),
		sizeof(info::binaries::slot_option),
		sizeof(info::binaries::configuration),
		sizeof(info::binaries::configuration_setting),
		sizeof(info::binaries::configuration_condition),
		sizeof(info::binaries::software_list),
		sizeof(info::binaries::ram_option)
	};

	uint64_t result = 0;
	for (int i = 0; i < std::size(sizes); i++)
	{
		result *= 4729;			// arbitrary prime number
		result ^= sizes[i];
	}
	return result;
}


//-------------------------------------------------
//  database::reset
//-------------------------------------------------

void info::database::reset()
{
	m_state = State();
	m_loaded_strings.clear();
	m_version = &util::g_empty_string;
	onChanged();
}


//-------------------------------------------------
//  database::addOnChangedHandler
//-------------------------------------------------

void info::database::addOnChangedHandler(std::function<void()> &&onChanged)
{
	m_onChangedHandlers.push_back(std::move(onChanged));
}


//-------------------------------------------------
//  database::onChanged
//-------------------------------------------------

void info::database::onChanged()
{
	for (const std::function<void()> &func : m_onChangedHandlers)
		func();
}


//-------------------------------------------------
//  database::get_string
//-------------------------------------------------

const QString &info::database::get_string(std::uint32_t offset) const
{
	if ((size_t)m_state.m_string_table_offset + offset >= m_state.m_data.size())
		throw false;

	auto iter = m_loaded_strings.find(offset);
	if (iter != m_loaded_strings.end())
		return iter->second;

	const char *string = getStringFromData(m_state, util::safe_static_cast<std::uint32_t>(offset));
	m_loaded_strings.emplace(offset, QString::fromUtf8(string));
	return m_loaded_strings.find(offset)->second;
}


//-------------------------------------------------
//  database::getStringFromData - raw string
//	retrieval that bypasses loaded strings
//-------------------------------------------------

const char *info::database::getStringFromData(const State &state, std::uint32_t offset)
{
	// sanity check
	if (offset >= state.m_data.size() || (state.m_string_table_offset + offset) >= state.m_data.size())
		return "";	// should not happen with a valid info DB

	// access the data
	return reinterpret_cast<const char *>(&state.m_data.data()[state.m_string_table_offset + offset]);
}


//-------------------------------------------------
//  machine::find_device
//-------------------------------------------------

std::optional<info::device> info::machine::find_device(const QString &tag) const
{
	// find the device
	auto iter = std::find_if(
		devices().cbegin(),
		devices().cend(),
		[&tag](info::device dev) { return tag == dev.tag(); });

	// if we found a device, return the interface
	return iter != devices().cend()
		? *iter
		: std::optional<info::device>();
}


//-------------------------------------------------
//  slot_option::machine
//-------------------------------------------------

std::optional<info::machine> info::slot_option::machine() const
{
	return db().find_machine(devname());

}


//-------------------------------------------------
//  database::find_machine
//-------------------------------------------------

std::optional<info::machine> info::database::find_machine(const QString &machine_name) const
{
	auto iter = std::lower_bound(
		machines().begin(),
		machines().end(),
		machine_name,
		[](const info::machine &a, const QString &b)
		{
			return a.name() < b;
		});
	return iter != machines().end() && iter->name() == machine_name
		? *iter
		: std::optional<info::machine>();
}


//-------------------------------------------------
//  info::database::State ctor
//-------------------------------------------------

info::database::State::State()
	: m_string_table_offset(0)
{
}
