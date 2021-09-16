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
//  CONSTANTS
//**************************************************************************

static const char8_t s_smallStringChars[] = u8"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 ";


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
//  tryGetQStringFromCharSpan
//-------------------------------------------------

static std::optional<QString> tryGetQStringFromCharSpan(const std::span<const char> &span)
{
	auto iter = std::find(span.begin(), span.end(), '\0');
	if (iter == span.end())
		return { };
	return QString::fromUtf8(&span[0], iter - span.begin());
}


//-------------------------------------------------
//  getQStringFromCharSpan
//-------------------------------------------------

static QString getQStringFromCharSpan(const std::span<const char> &span)
{
	std::optional<QString> result = tryGetQStringFromCharSpan(span);
	if (!result)
		throw false;
	return std::move(*result);
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
	newState.m_biossets_position					= getPosition<binaries::biosset>(cursor, hdr.m_biossets_count);
	newState.m_roms_position						= getPosition<binaries::rom>(cursor, hdr.m_roms_count);
	newState.m_disks_position						= getPosition<binaries::disk>(cursor, hdr.m_disks_count);
	newState.m_devices_position						= getPosition<binaries::device>(cursor, hdr.m_devices_count);
	newState.m_slots_position						= getPosition<binaries::slot>(cursor, hdr.m_slots_count);
	newState.m_slot_options_position				= getPosition<binaries::slot_option>(cursor, hdr.m_slot_options_count);
	newState.m_features_position					= getPosition<binaries::feature>(cursor, hdr.m_features_count);
	newState.m_chips_position						= getPosition<binaries::chip>(cursor, hdr.m_chips_count);
	newState.m_displays_position					= getPosition<binaries::display>(cursor, hdr.m_displays_count);
	newState.m_samples_position						= getPosition<binaries::sample>(cursor, hdr.m_samples_count);
	newState.m_configurations_position				= getPosition<binaries::configuration>(cursor, hdr.m_configurations_count);
	newState.m_configuration_settings_position		= getPosition<binaries::configuration_setting>(cursor, hdr.m_configuration_settings_count);
	newState.m_configuration_conditions_position	= getPosition<binaries::configuration_condition>(cursor, hdr.m_configuration_conditions_count);
	newState.m_software_lists_position				= getPosition<binaries::software_list>(cursor, hdr.m_software_lists_count);
	newState.m_ram_options_position					= getPosition<binaries::ram_option>(cursor, hdr.m_ram_options_count);
	newState.m_string_table_offset					= static_cast<std::uint32_t>(cursor);

	// sanity check the string table
	if (newState.m_data.size() < (size_t)newState.m_string_table_offset + 1)
		return false;
	if (!unaligned_check(&newState.m_data[(size_t)newState.m_string_table_offset], binaries::MAGIC_STRINGTABLE_BEGIN))
		return false;
	if (newState.m_data[newState.m_data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END) - 1] != '\0')
		return false;
	if (!unaligned_check(&newState.m_data[newState.m_data.size() - sizeof(binaries::MAGIC_STRINGTABLE_END)], binaries::MAGIC_STRINGTABLE_END))
		return false;
	if (newState.m_string_table_offset != cursor)
		return false;

	// version check if appropriate
	std::optional<std::span<const char>> buildVersion = tryGetDataSpan<char>(newState, newState.m_string_table_offset + hdr.m_build_strindex);
	if (!buildVersion)
		return false;
	if (!expected_version.isEmpty() && expected_version != tryGetQStringFromCharSpan(*buildVersion))
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
		sizeof(info::binaries::biosset),
		sizeof(info::binaries::rom),
		sizeof(info::binaries::disk),
		sizeof(info::binaries::device),
		sizeof(info::binaries::slot),
		sizeof(info::binaries::slot_option),
		sizeof(info::binaries::feature),
		sizeof(info::binaries::chip),
		sizeof(info::binaries::display),
		sizeof(info::binaries::sample),
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
	// do we have this string in our loaded string cache?
	auto iter = m_loaded_strings.find(offset);
	if (iter != m_loaded_strings.end())
		return iter->second;

	QString string;
	std::optional<std::array<char8_t, 6>> smallString = tryDecodeAsSmallString(offset);
	if (smallString)
	{
		// this was a small string
		string = QString::fromUtf8(&(*smallString)[0]);
	}
	else
	{
		// perform a string table lookup
		std::span<const char> utf8String = getDataSpan<char>(m_state.m_string_table_offset + offset);
		string = getQStringFromCharSpan(utf8String);
	}

	// deposit this image in our cache
	m_loaded_strings.emplace(offset, std::move(string));

	// and return a reference out of our cache
	return m_loaded_strings.find(offset)->second;
}


//-------------------------------------------------
//  database::tryEncodeSmallStringChar
//-------------------------------------------------

std::optional<std::uint32_t> info::database::tryEncodeSmallStringChar(char8_t ch)
{
	const char8_t *p = (const char8_t *) memchr(s_smallStringChars, ch, std::size(s_smallStringChars));
	return p
		? (std::uint32_t)(p - s_smallStringChars)
		: std::optional<std::uint32_t>();
}


//-------------------------------------------------
//  database::tryEncodeAsSmallString
//-------------------------------------------------

std::optional<std::uint32_t> info::database::tryEncodeAsSmallString(std::u8string_view s)
{
	if (s.size() > 5)
		return { };

	std::optional<std::uint32_t> parts[5] =
	{
		tryEncodeSmallStringChar(s.size() >= 1 ? s[0] : '\0'),
		tryEncodeSmallStringChar(s.size() >= 2 ? s[1] : '\0'),
		tryEncodeSmallStringChar(s.size() >= 3 ? s[2] : '\0'),
		tryEncodeSmallStringChar(s.size() >= 4 ? s[3] : '\0'),
		tryEncodeSmallStringChar(s.size() >= 5 ? s[4] : '\0')
	};

	std::uint32_t result = 0xC0000000;
	for (int i = 0; i < std::size(parts); i++)
	{
		if (!parts[i])
			return { };
		result |= *(parts[i]) << i * 6;
	}

	return result;
}


//-------------------------------------------------
//  database::tryDecodeAsSmallString
//-------------------------------------------------

std::optional<std::array<char8_t, 6>> info::database::tryDecodeAsSmallString(std::uint32_t value)
{
	std::optional<std::array<char8_t, 6>> result = { };

	// small strings have the high two bits set; check for that first
	if ((value & 0xC0000000) == 0xC0000000)
	{
		// unpack this into a buffer
		char8_t buffer[6] = { 0, };
		for (int i = 0; i < 5; i++)
			buffer[i] = s_smallStringChars[(value >> (i * 6)) & 0x3F];

		// and get the string
		result = std::to_array(buffer);
	}
	return result;
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
//  machine::find_chip
//-------------------------------------------------

std::optional<info::chip> info::machine::find_chip(const QString &chipName) const
{
	// find the device
	auto iter = std::find_if(
		chips().cbegin(),
		chips().cend(),
		[&chipName](info::chip chip) { return chipName == chip.name(); });

	// if we found a device, return the interface
	return iter != chips().cend()
		? *iter
		: std::optional<info::chip>();
}


//-------------------------------------------------
//  machine::clone_of
//-------------------------------------------------

std::optional<info::machine> info::machine::clone_of() const
{
	return inner().m_clone_of_machindex < db().machines().size()
		? db().machines()[inner().m_clone_of_machindex]
		: std::optional<info::machine>();
}


//-------------------------------------------------
//  machine::rom_of
//-------------------------------------------------

std::optional<info::machine> info::machine::rom_of() const
{
	return inner().m_rom_of_machindex < db().machines().size()
		? db().machines()[inner().m_rom_of_machindex]
		: std::optional<info::machine>();
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
