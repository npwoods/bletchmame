/***************************************************************************

	info_builder.cpp

	Code to build MAME info DB

***************************************************************************/

// bletchmame headers
#include "info_builder.h"
#include "perfprofiler.h"
#include "throttler.h"

// standard headers
#include <chrono>
#include <cmath>
#include <ranges>


//**************************************************************************
//  LOCALS
//**************************************************************************

static const util::enum_parser<info::rom::dump_status_t> s_dump_status_parser =
{
	{ "baddump", info::rom::dump_status_t::BADDUMP },
	{ "nodump", info::rom::dump_status_t::NODUMP },
	{ "good", info::rom::dump_status_t::GOOD }
};


static const util::enum_parser<info::software_list::status_type> s_status_parser =
{
	{ "original", info::software_list::status_type::ORIGINAL, },
	{ "compatible", info::software_list::status_type::COMPATIBLE }
};


static const util::enum_parser<info::configuration_condition::relation_t> s_relation_parser =
{
	{ "eq", info::configuration_condition::relation_t::EQ },
	{ "ne", info::configuration_condition::relation_t::NE },
	{ "gt", info::configuration_condition::relation_t::GT },
	{ "le", info::configuration_condition::relation_t::LE },
	{ "lt", info::configuration_condition::relation_t::LT },
	{ "ge", info::configuration_condition::relation_t::GE }
};


static const util::enum_parser<info::feature::type_t> s_feature_type_parser =
{
	{ "protection",	info::feature::type_t::PROTECTION },
	{ "timing",		info::feature::type_t::TIMING },
	{ "graphics",	info::feature::type_t::GRAPHICS },
	{ "palette",	info::feature::type_t::PALETTE },
	{ "sound",		info::feature::type_t::SOUND },
	{ "capture",	info::feature::type_t::CAPTURE },
	{ "camera",		info::feature::type_t::CAMERA },
	{ "microphone",	info::feature::type_t::MICROPHONE },
	{ "controls",	info::feature::type_t::CONTROLS },
	{ "keyboard",	info::feature::type_t::KEYBOARD },
	{ "mouse",		info::feature::type_t::MOUSE },
	{ "media",		info::feature::type_t::MEDIA },
	{ "disk",		info::feature::type_t::DISK },
	{ "printer",	info::feature::type_t::PRINTER },
	{ "tape",		info::feature::type_t::TAPE },
	{ "punch",		info::feature::type_t::PUNCH },
	{ "drum",		info::feature::type_t::DRUM },
	{ "rom",		info::feature::type_t::ROM },
	{ "comms",		info::feature::type_t::COMMS },
	{ "lan",		info::feature::type_t::LAN },
	{ "wan",		info::feature::type_t::WAN },
};


static const util::enum_parser<info::feature::quality_t> s_feature_quality_parser =
{
	{ "unemulated",	info::feature::quality_t::UNEMULATED },
	{ "imperfect",	info::feature::quality_t::IMPERFECT }
};


static const util::enum_parser<info::chip::type_t> s_chip_type_parser =
{
	{ "cpu", info::chip::type_t::CPU },
	{ "audio", info::chip::type_t::AUDIO }
};


static const util::enum_parser<info::display::type_t> s_display_type_parser =
{
	{ "unknown", info::display::type_t::UNKNOWN },
	{ "raster", info::display::type_t::RASTER },
	{ "vector", info::display::type_t::VECTOR },
	{ "lcd", info::display::type_t::LCD },
	{ "svg", info::display::type_t::SVG }
};


static const util::enum_parser<info::display::rotation_t> s_display_rotation_parser =
{
	{ "0", info::display::rotation_t::ROT0 },
	{ "90", info::display::rotation_t::ROT90 },
	{ "180", info::display::rotation_t::ROT180 },
	{ "270", info::display::rotation_t::ROT270 }
};


static const util::enum_parser<info::machine::driver_quality_t> s_driver_quality_parser =
{
	{ "good", info::machine::driver_quality_t::GOOD },
	{ "imperfect", info::machine::driver_quality_t::IMPERFECT },
	{ "preliminary", info::machine::driver_quality_t::PRELIMINARY }
};


static const util::enum_parser<bool> s_supported_parser =
{
	{ "supported", true },
	{ "unsupported", false }
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  to_uint32
//-------------------------------------------------

template<typename T>
static std::uint32_t to_uint32(T &&value)
{
	std::uint32_t new_value = static_cast<std::uint32_t>(value);
	if (new_value != value)
		throw std::logic_error("Array size cannot fit in 32 bits");
	return new_value;
}


//-------------------------------------------------
//  writeContainerData
//-------------------------------------------------

template<typename T>
static void writeContainerData(QIODevice &stream, std::span<const T> container)
{
	stream.write((const char *)container.data(), container.size() * sizeof(T));
}


//-------------------------------------------------
//  writeContainerData
//-------------------------------------------------

template<typename T>
static void writeContainerData(QIODevice &stream, const std::vector<T> &container)
{
	writeContainerData(stream, std::span<const T>(container));
}


//-------------------------------------------------
//  encodeBool
//-------------------------------------------------

static constexpr std::uint8_t encodeBool(std::optional<bool> b, std::uint8_t defaultValue = 0xFF)
{
	return b
		? (*b ? 0x01 : 0x00)
		: defaultValue;
}


//-------------------------------------------------
//  encodeEnum
//-------------------------------------------------

template<typename T>
static constexpr std::uint8_t encodeEnum(std::optional<T> &&value, std::uint8_t defaultValue = 0)
{
	return value
		? (std::uint8_t) *value
		: defaultValue;
}


//-------------------------------------------------
//  binaryFromHex
//-------------------------------------------------

template<int N>
static bool binaryFromHex(std::uint8_t (&dest)[N], const XmlParser::Attribute &attr)
{
	std::optional<std::u8string_view> hex = attr.as<std::u8string_view>();

	std::optional<std::array<std::uint8_t, N>> result;
	if (hex)
		result = util::fixedByteArrayFromHex<N>(*hex);

	if (result)
		std::copy(result->begin(), result->end(), dest);
	else
		std::fill(dest, dest + N, 0);
	return bool(result);
}


//-------------------------------------------------
//  binaryWipe
//-------------------------------------------------

template<class T>
static void binaryWipe(T &x)
{
	// this is to ensure that the binaries that info DB built are deterministic; in
	// practice all bytes in the structure should be replaced except for padding
	memset(&x, 0xCD, sizeof(x));
}


//-------------------------------------------------
//  process_xml()
//-------------------------------------------------

bool info::database_builder::process_xml(QIODevice &input, QString &error_message, const ProcessXmlCallback &progressCallback) noexcept
{
	using namespace std::chrono_literals;

	// sanity check; ensure we're fresh
	assert(m_machines.empty());
	assert(m_devices.empty());

	// progress reporting
	Throttler throttler(100ms);
	auto reportProgressIfAppropriate = [this, &throttler, &progressCallback](const info::binaries::machine &machine)
	{
		// is it time to report progress?
		if (throttler.check() && progressCallback)
		{
			// it is, report it
			info::database_builder::string_table::SsoBuffer nameSso, descSso;
			const char8_t *name = m_strings.lookup(machine.m_name_strindex, nameSso);
			const char8_t *desc = m_strings.lookup(machine.m_description_strindex, descSso);
			progressCallback(util::safe_static_cast<int>(m_machines.size()), name, desc);
		}
	};

	// prepare header and magic variables
	info::binaries::header header = { 0, };
	header.m_magic = info::binaries::MAGIC_HDR;
	header.m_sizes_hash = info::database::calculate_sizes_hash();

	// reserve space based on what we know about MAME 0.239
	m_biossets.reserve(36000);					// 34067 bios sets
	m_roms.reserve(350000);						// 329670 roms
	m_disks.reserve(1400);						// 1193 disks
	m_machines.reserve(48000);					// 44092 machines
	m_devices.reserve(12000);					// 10738 devices
	m_features.reserve(22000);					// 20412 features
	m_chips.reserve(190000);					// 174679 chips
	m_displays.reserve(22000);					// 20194 displays
	m_samples.reserve(21000);					// 19402 samples
	m_configurations.reserve(600000);			// 553822 configurations
	m_configuration_conditions.reserve(7500);	// 6853 conditions
	m_configuration_settings.reserve(1700000);	// 1639291 settings
	m_software_lists.reserve(6800);				// 6337 software lists
	m_ram_options.reserve(6800);				// 6383 ram options

	// parse the -listxml output
	XmlParser xml;
	std::u8string current_device_extensions;
	std::uint32_t empty_strindex = m_strings.get(u8"");
	xml.onElementBegin({ "mame" }, [this, &header](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [build] = attributes.get("build");
		header.m_build_strindex = m_strings.get(build);
	});
	xml.onElementBegin({ "mame", "machine" }, [this, empty_strindex](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);

		const auto [runnable, name, sourcefile, cloneof, romof, isbios, isdevice, ismechanical] = attributes.get(
			"runnable", "name", "sourcefile", "cloneof", "romof", "isbios", "isdevice", "ismechanical");

		info::binaries::machine &machine = m_machines.emplace_back();
		binaryWipe(machine);
		machine.m_runnable				= encodeBool(runnable.as<bool>().value_or(true));
		machine.m_name_strindex			= m_strings.get(name);
		machine.m_sourcefile_strindex	= m_strings.get(sourcefile);
		machine.m_clone_of_machindex	= m_strings.get(cloneof);		// string index for now; changes to machine index later
		machine.m_rom_of_machindex		= m_strings.get(romof);			// string index for now; changes to machine index later
		machine.m_is_bios				= encodeBool(isbios.as<bool>());
		machine.m_is_device				= encodeBool(isdevice.as<bool>());
		machine.m_is_mechanical			= encodeBool(ismechanical.as<bool>());
		machine.m_biossets_index		= to_uint32(m_biossets.size());
		machine.m_biossets_count		= 0;
		machine.m_roms_index			= to_uint32(m_roms.size());
		machine.m_roms_count			= 0;
		machine.m_disks_index			= to_uint32(m_disks.size());
		machine.m_disks_count			= 0;
		machine.m_features_index		= to_uint32(m_features.size());
		machine.m_features_count		= 0;
		machine.m_chips_index			= to_uint32(m_chips.size());
		machine.m_chips_count			= 0;
		machine.m_displays_index		= to_uint32(m_displays.size());
		machine.m_displays_count		= 0;
		machine.m_samples_index			= to_uint32(m_samples.size());
		machine.m_samples_count			= 0;
		machine.m_configurations_index	= to_uint32(m_configurations.size());
		machine.m_configurations_count	= 0;
		machine.m_software_lists_index	= to_uint32(m_software_lists.size());
		machine.m_software_lists_count	= 0;
		machine.m_ram_options_index		= to_uint32(m_ram_options.size());
		machine.m_ram_options_count		= 0;
		machine.m_devices_index			= to_uint32(m_devices.size());
		machine.m_devices_count			= 0;
		machine.m_slots_index			= to_uint32(m_slots.size());
		machine.m_slots_count			= 0;
		machine.m_description_strindex	= empty_strindex;
		machine.m_year_strindex			= empty_strindex;
		machine.m_manufacturer_strindex = empty_strindex;
		machine.m_quality_status		= 0;
		machine.m_quality_emulation		= 0;
		machine.m_quality_cocktail		= 0;
		machine.m_save_state_supported	= encodeBool(std::nullopt);
		machine.m_unofficial			= encodeBool(std::nullopt);
		machine.m_incomplete			= encodeBool(std::nullopt);
		machine.m_sound_channels		= ~0;
	});
	xml.onElementEnd({ "mame", "machine", "description" }, [this, &reportProgressIfAppropriate](std::u8string &&content)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		util::last(m_machines).m_description_strindex = m_strings.get(content);
		reportProgressIfAppropriate(util::last(m_machines));
	});
	xml.onElementEnd({ "mame", "machine", "year" }, [this](std::u8string &&content)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		util::last(m_machines).m_year_strindex = m_strings.get(content);
	});
	xml.onElementEnd({ "mame", "machine", "manufacturer" }, [this](std::u8string &&content)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		util::last(m_machines).m_manufacturer_strindex = m_strings.get(content);
	});
	xml.onElementBegin({ "mame", "machine", "biosset" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, description, is_default] = attributes.get("name", "description", "default");

		info::binaries::biosset &biosset = m_biossets.emplace_back();
		binaryWipe(biosset);
		biosset.m_name_strindex				= m_strings.get(name);
		biosset.m_description_strindex		= m_strings.get(description);
		biosset.m_default					= encodeBool(is_default.as<bool>().value_or(false));
		util::last(m_machines).m_biossets_count++;
	});
	xml.onElementBegin({ "mame", "machine", "rom" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, bios, size, crc, sha1, merge, region, offset, status, optional] = attributes.get(
			"name", "bios", "size", "crc", "sha1", "merge", "region", "offset", "status", "optional");

		info::binaries::rom &rom = m_roms.emplace_back();
		binaryWipe(rom);
		rom.m_name_strindex					= m_strings.get(name);
		rom.m_bios_strindex					= m_strings.get(bios);
		rom.m_size							= size.as<std::uint32_t>().value_or(0);
		binaryFromHex(rom.m_crc32,			  crc);
		binaryFromHex(rom.m_sha1,			  sha1);
		rom.m_merge_strindex				= m_strings.get(merge);
		rom.m_region_strindex				= m_strings.get(region);
		rom.m_offset						= offset.as<std::uint64_t>(16).value_or(0);
		rom.m_status						= encodeEnum(status.as<info::rom::dump_status_t>(s_dump_status_parser));
		rom.m_optional						= encodeBool(optional.as<bool>().value_or(false));
		util::last(m_machines).m_roms_count++;
	});
	xml.onElementBegin({ "mame", "machine", "disk" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, sha1, merge, region, index, writable, status, optional] = attributes.get(
			"name", "sha1", "merge", "region", "index", "writable", "status", "optional");

		info::binaries::disk &disk = m_disks.emplace_back();
		binaryWipe(disk);
		disk.m_name_strindex				= m_strings.get(name);
		binaryFromHex(disk.m_sha1,			  sha1);
		disk.m_merge_strindex				= m_strings.get(merge);
		disk.m_region_strindex				= m_strings.get(region);
		disk.m_index						= index.as<std::uint32_t>().value_or(0);
		disk.m_writable						= encodeBool(writable.as<bool>().value_or(false));
		disk.m_status						= encodeEnum(status.as<info::rom::dump_status_t>(s_dump_status_parser));
		disk.m_optional						= encodeBool(optional.as<bool>().value_or(false));
		util::last(m_machines).m_disks_count++;
	});
	xml.onElementBegin({ "mame", "machine", "feature" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [type, status, overall] = attributes.get("type", "status", "overall");

		info::binaries::feature &feature = m_features.emplace_back();
		binaryWipe(feature);
		feature.m_type		= encodeEnum(type.as<info::feature::type_t>			(s_feature_type_parser));
		feature.m_status	= encodeEnum(status.as<info::feature::quality_t>	(s_feature_quality_parser));
		feature.m_overall	= encodeEnum(overall.as<info::feature::quality_t>	(s_feature_quality_parser));
		util::last(m_machines).m_features_count++;
	});
	xml.onElementBegin({ "mame", "machine", "chip" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [type, name, tag, clock] = attributes.get("type", "name", "tag", "clock");

		info::binaries::chip &chip = m_chips.emplace_back();
		binaryWipe(chip);
		chip.m_type				= encodeEnum(type.as<info::chip::type_t>(s_chip_type_parser));
		chip.m_name_strindex	= m_strings.get(name);
		chip.m_tag_strindex		= m_strings.get(tag);
		chip.m_clock			= clock.as<std::uint64_t>().value_or(0);

		util::last(m_machines).m_chips_count++;
	});
	xml.onElementBegin({ "mame", "machine", "display" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [tag, width, height, refresh, pixclock, htotal, hbend, hbstart, vtotal, vbend, vbstart, type, rotate, flipx] = attributes.get(
			"tag", "width", "height", "refresh", "pixclock", "htotal", "hbend", "hbstart", "vtotal", "vbend", "vbstart", "type", "rotate", "flipx");

		info::binaries::display &display = m_displays.emplace_back();
		binaryWipe(display);
		display.m_tag_strindex	= m_strings.get(tag);
		display.m_width			= width.as<std::uint32_t>().value_or(~0);
		display.m_height		= height.as<std::uint32_t>().value_or(~0);
		display.m_refresh		= refresh.as<float>().value_or(NAN);
		display.m_pixclock		= pixclock.as<std::uint64_t>().value_or(~0);
		display.m_htotal		= htotal.as<std::uint32_t>().value_or(~0);
		display.m_hbend			= hbend.as<std::uint32_t>().value_or(~0);
		display.m_hbstart		= hbstart.as<std::uint32_t>().value_or(~0);
		display.m_vtotal		= vtotal.as<std::uint32_t>().value_or(~0);
		display.m_vbend			= vbend.as<std::uint32_t>().value_or(~0);
		display.m_vbstart		= vbstart.as<std::uint32_t>().value_or(~0);
		display.m_type			= encodeEnum(type.as<info::display::type_t>(s_display_type_parser));
		display.m_rotate		= encodeEnum(rotate.as<info::display::rotation_t>(s_display_rotation_parser));
		display.m_flipx			= encodeBool(flipx.as<bool>());
		util::last(m_machines).m_displays_count++;
	});
	xml.onElementBegin({ "mame", "machine", "sample" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name] = attributes.get("name");

		info::binaries::sample &sample = m_samples.emplace_back();
		binaryWipe(sample);
		sample.m_name_strindex	= m_strings.get(name);
		util::last(m_machines).m_samples_count++;
	});
	xml.onElementBegin({ { "mame", "machine", "configuration" },
						 { "mame", "machine", "dipswitch" } }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, tag, mask] = attributes.get("name", "tag", "mask");

		info::binaries::configuration &configuration = m_configurations.emplace_back();
		binaryWipe(configuration);
		configuration.m_name_strindex					= m_strings.get(name);
		configuration.m_tag_strindex					= m_strings.get(tag);
		configuration.m_mask							= mask.as<std::uint32_t>().value_or(0);
		configuration.m_configuration_settings_index	= to_uint32(m_configuration_settings.size());
		configuration.m_configuration_settings_count	= 0;
	
		util::last(m_machines).m_configurations_count++;
	});
	xml.onElementBegin({ { "mame", "machine", "configuration", "confsetting" },
						 { "mame", "machine", "dipswitch", "dipvalue" } }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, value] = attributes.get("name", "value");

		info::binaries::configuration_setting &configuration_setting = m_configuration_settings.emplace_back();
		binaryWipe(configuration_setting);
		configuration_setting.m_name_strindex		= m_strings.get(name);
		configuration_setting.m_conditions_index	= to_uint32(m_configuration_conditions.size());
		configuration_setting.m_value				= value.as<std::uint32_t>().value_or(0);

		util::last(m_configurations).m_configuration_settings_count++;
	});
	xml.onElementBegin({ { "mame", "machine", "configuration", "confsetting", "condition" },
						 { "mame", "machine", "dipswitch", "dipvalue", "condition" } }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [tag, relation, mask, value] = attributes.get("tag", "relation", "mask", "value");

		info::binaries::configuration_condition &configuration_condition = m_configuration_conditions.emplace_back();
		binaryWipe(configuration_condition);
		configuration_condition.m_tag_strindex			= m_strings.get(tag);
		configuration_condition.m_relation				= encodeEnum(relation.as<info::configuration_condition::relation_t>(s_relation_parser));
		configuration_condition.m_mask					= mask.as<std::uint32_t>().value_or(0);
		configuration_condition.m_value					= value.as<std::uint32_t>().value_or(0);
	});
	xml.onElementBegin({ "mame", "machine", "device" }, [this, &current_device_extensions, empty_strindex](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [type, tag, interface, mandatory] = attributes.get("type", "tag", "interface", "mandatory");

		info::binaries::device &device = m_devices.emplace_back();
		binaryWipe(device);
		device.m_type_strindex			= m_strings.get(type);
		device.m_tag_strindex			= m_strings.get(tag);
		device.m_interface_strindex		= m_strings.get(interface);
		device.m_mandatory				= encodeBool(mandatory.as<bool>().value_or(false));
		device.m_instance_name_strindex	= empty_strindex;
		device.m_extensions_strindex	= empty_strindex;

		current_device_extensions.clear();

		util::last(m_machines).m_devices_count++;
	});
	xml.onElementBegin({ "mame", "machine", "device", "instance" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name] = attributes.get("name");
		util::last(m_devices).m_instance_name_strindex = m_strings.get(name);
	});
	xml.onElementBegin({ "mame", "machine", "device", "extension" }, [&current_device_extensions](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name] = attributes.get("name");

		if (name)
		{
			current_device_extensions.append(*name.as<std::u8string_view>());
			current_device_extensions.append(u8",");
		}
	});
	xml.onElementEnd({ "mame", "machine", "device" }, [this, &current_device_extensions]()
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		if (!current_device_extensions.empty())
			util::last(m_devices).m_extensions_strindex = m_strings.get(current_device_extensions);
	});
	xml.onElementBegin({ "mame", "machine", "driver" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [status, emulation, cocktail, savestate, unofficial, incomplete] = attributes.get("status", "emulation", "cocktail", "savestate", "unofficial", "incomplete");

		info::binaries::machine &machine = util::last(m_machines);
		machine.m_quality_status		= encodeEnum(status.as<info::machine::driver_quality_t>(s_driver_quality_parser),		machine.m_quality_status);
		machine.m_quality_emulation		= encodeEnum(emulation.as<info::machine::driver_quality_t>(s_driver_quality_parser),	machine.m_quality_emulation);
		machine.m_quality_cocktail		= encodeEnum(cocktail.as<info::machine::driver_quality_t>(s_driver_quality_parser),		machine.m_quality_cocktail);
		machine.m_save_state_supported	= encodeBool(savestate.as<bool>(s_supported_parser),									machine.m_save_state_supported);
		machine.m_unofficial			= encodeBool(unofficial.as<bool>(),														machine.m_unofficial);
		machine.m_incomplete			= encodeBool(incomplete.as<bool>(),														machine.m_incomplete);
	});
	xml.onElementBegin({ "mame", "machine", "slot" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name] = attributes.get("name");

		info::binaries::slot &slot = m_slots.emplace_back();
		binaryWipe(slot);
		slot.m_name_strindex					= m_strings.get(name);
		slot.m_slot_options_index				= to_uint32(m_slot_options.size());
		slot.m_slot_options_count				= 0;
		util::last(m_machines).m_slots_count++;
	});
	xml.onElementBegin({ "mame", "machine", "slot", "slotoption" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, devname, is_default] = attributes.get("name", "devname", "default");

		info::binaries::slot_option &slot_option = m_slot_options.emplace_back();
		binaryWipe(slot_option);
		slot_option.m_name_strindex				= m_strings.get(name);
		slot_option.m_devname_strindex			= m_strings.get(devname);
		slot_option.m_is_default				= encodeBool(is_default.as<bool>().value_or(false));
		util::last(m_slots).m_slot_options_count++;
	});
	xml.onElementBegin({ "mame", "machine", "softwarelist" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, filter, status] = attributes.get("name", "filter", "status");

		info::binaries::software_list &software_list = m_software_lists.emplace_back();
		binaryWipe(software_list);
		software_list.m_name_strindex			= m_strings.get(name);
		software_list.m_filter_strindex			= m_strings.get(filter);
		software_list.m_status					= encodeEnum(status.as<info::software_list::status_type>(s_status_parser));
		util::last(m_machines).m_software_lists_count++;
	});
	xml.onElementBegin({ "mame", "machine", "ramoption" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [name, is_default] = attributes.get("name", "default");

		info::binaries::ram_option &ram_option = m_ram_options.emplace_back();
		binaryWipe(ram_option);
		ram_option.m_name_strindex				= m_strings.get(name);
		ram_option.m_is_default					= encodeBool(is_default.as<bool>().value_or(false));
		ram_option.m_value						= 0;
		util::last(m_machines).m_ram_options_count++;
	});
	xml.onElementEnd({ "mame", "machine", "ramoption" }, [this](std::u8string &&content)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		bool ok;
		unsigned long val = util::toQString(content).toULong(&ok);
		util::last(m_ram_options).m_value = ok ? val : 0;
	});
	xml.onElementBegin({ "mame", "machine", "sound" }, [this](const XmlParser::Attributes &attributes)
	{
		ProfilerScope prof(CURRENT_FUNCTION);
		const auto [channels] = attributes.get("channels");

		info::binaries::machine &machine = util::last(m_machines);
		machine.m_sound_channels		= channels.as<std::uint8_t>().value_or(~0);
	});

	// parse!
	bool success;
	try
	{
		success = xml.parse(input);
	}
	catch (std::exception &ex)
	{
		// did an exception (probably thrown by to_uint32) get thrown?
		error_message = ex.what();
		return false;
	}
	if (!success)
	{
		// now check for XML parsing errors; this is likely the result of somebody aborting the DB rebuild, but
		// it is the caller's responsibility to handle that situation
		error_message = xml.errorMessagesSingleString();
		return false;
	}

	// final magic bytes on string table
	m_strings.embed_value(info::binaries::MAGIC_STRINGTABLE_END);

	// finalize the header
	header.m_machines_count					= to_uint32(m_machines.size());
	header.m_biossets_count					= to_uint32(m_biossets.size());
	header.m_roms_count						= to_uint32(m_roms.size());
	header.m_disks_count					= to_uint32(m_disks.size());
	header.m_devices_count					= to_uint32(m_devices.size());
	header.m_slots_count					= to_uint32(m_slots.size());
	header.m_slot_options_count				= to_uint32(m_slot_options.size());
	header.m_features_count					= to_uint32(m_features.size());
	header.m_chips_count					= to_uint32(m_chips.size());
	header.m_displays_count					= to_uint32(m_displays.size());
	header.m_samples_count					= to_uint32(m_samples.size());
	header.m_configurations_count			= to_uint32(m_configurations.size());
	header.m_configuration_settings_count	= to_uint32(m_configuration_settings.size());
	header.m_configuration_conditions_count	= to_uint32(m_configuration_conditions.size());
	header.m_software_lists_count			= to_uint32(m_software_lists.size());
	header.m_ram_options_count				= to_uint32(m_ram_options.size());

	// and salt it
	m_salted_header = util::salt(header, info::binaries::salt());

	// sort machines by name to facilitate lookups
	std::sort(
		m_machines.begin(),
		m_machines.end(),
		[this](const binaries::machine &a, const binaries::machine &b)
		{
			string_table::SsoBuffer ssoBufferA, ssoBufferB;
			std::u8string_view aText = m_strings.lookup(a.m_name_strindex, ssoBufferA);
			std::u8string_view bText = m_strings.lookup(b.m_name_strindex, ssoBufferB);
			return aText < bText;
		});

	// build a machine index map
	std::unordered_map<std::uint32_t, std::uint32_t> machineIndexMap;
	machineIndexMap.reserve(m_machines.size() + 1);
	machineIndexMap.emplace(m_strings.get(std::u8string()), ~0);
	for (auto iter = m_machines.begin(); iter != m_machines.end(); iter++)
	{
		machineIndexMap.emplace(iter->m_name_strindex, iter - m_machines.begin());
	}

	// helper to perform machine index lookups
	auto machineIndexFromStringIndex = [&machineIndexMap](std::uint32_t stringIndex)
	{
		auto iter = machineIndexMap.find(stringIndex);
		return iter != machineIndexMap.end()
			? iter->second
			: ~0;	// should never happen unless -listxml is returning bad results
	};

	// and change clone_of and rom_of to be machine indexes, using the map we have above
	for (info::binaries::machine &machine : m_machines)
	{
		machine.m_clone_of_machindex = machineIndexFromStringIndex(machine.m_clone_of_machindex);
		machine.m_rom_of_machindex = machineIndexFromStringIndex(machine.m_rom_of_machindex);
	}

	// success!
	error_message.clear();
	return true;
}


//-------------------------------------------------
//  emit_info
//-------------------------------------------------

void info::database_builder::emit_info(QIODevice &output) const noexcept
{
	output.write((const char *) &m_salted_header, sizeof(m_salted_header));
	writeContainerData(output, m_machines);
	writeContainerData(output, m_biossets);
	writeContainerData(output, m_roms);
	writeContainerData(output, m_disks);
	writeContainerData(output, m_devices);
	writeContainerData(output, m_slots);
	writeContainerData(output, m_slot_options);
	writeContainerData(output, m_features);
	writeContainerData(output, m_chips);
	writeContainerData(output, m_displays);
	writeContainerData(output, m_samples);
	writeContainerData(output, m_configurations);
	writeContainerData(output, m_configuration_settings);
	writeContainerData(output, m_configuration_conditions);
	writeContainerData(output, m_software_lists);
	writeContainerData(output, m_ram_options);
	writeContainerData(output, m_strings.data());
}


//-------------------------------------------------
//  dump - dumps diagnostic information about what
//	was built
//-------------------------------------------------

void info::database_builder::dump() const noexcept
{
	dumpTableSizes();
	m_strings.dumpStringSizeDistribution();
	m_strings.dumpPrimaryBucketDistribution();
	m_strings.dumpSecondaryBucketDistribution();
}


//-------------------------------------------------
//  dumpTableSizes
//-------------------------------------------------

void info::database_builder::dumpTableSizes() const noexcept
{
	printf("\nDump of info::database_builder state:\n");
	printf("m_machines.size():                 %7lu\n", (unsigned long)m_machines.size());
	printf("m_biossets.size():                 %7lu\n", (unsigned long)m_biossets.size());
	printf("m_roms.size():                     %7lu\n", (unsigned long)m_roms.size());
	printf("m_disks.size():                    %7lu\n", (unsigned long)m_disks.size());
	printf("m_devices.size():                  %7lu\n", (unsigned long)m_devices.size());
	printf("m_slots.size():                    %7lu\n", (unsigned long)m_slots.size());
	printf("m_slot_options.size():             %7lu\n", (unsigned long)m_slot_options.size());
	printf("m_features.size():                 %7lu\n", (unsigned long)m_features.size());
	printf("m_chips.size():                    %7lu\n", (unsigned long)m_chips.size());
	printf("m_displays.size():                 %7lu\n", (unsigned long)m_displays.size());
	printf("m_samples.size():                  %7lu\n", (unsigned long)m_samples.size());
	printf("m_configurations.size():           %7lu\n", (unsigned long)m_configurations.size());
	printf("m_configuration_settings.size():   %7lu\n", (unsigned long)m_configuration_settings.size());
	printf("m_configuration_conditions.size(): %7lu\n", (unsigned long)m_configuration_conditions.size());
	printf("m_software_lists.size():           %7lu\n", (unsigned long)m_software_lists.size());
	printf("m_ram_options.size():              %7lu\n", (unsigned long)m_ram_options.size());
	printf("m_strings.data().size():           %7lu\n", (unsigned long)m_strings.data().size());
}


//-------------------------------------------------
//  string_table ctor
//-------------------------------------------------

info::database_builder::string_table::string_table() noexcept
{
	// reserve space based on expected size (see comments above)
	m_data.reserve(4500000);		// 4326752 bytes

	// embed the initial magic bytes
	embed_value(info::binaries::MAGIC_STRINGTABLE_BEGIN);

	// allocate the map buckets
	m_primaryMapBuckets = std::make_unique<decltype(m_primaryMapBuckets)::element_type>();
	std::ranges::fill(*m_primaryMapBuckets, 0);
	m_secondaryMapBuckets.reserve(60000);
}


//-------------------------------------------------
//  string_table::shrinkToFit
//-------------------------------------------------

void info::database_builder::string_table::shrinkToFit() noexcept
{
	// only actually done in unit tests
	m_data.shrink_to_fit();
}


//-------------------------------------------------
//  string_table::findBucket
//-------------------------------------------------

std::uint32_t &info::database_builder::string_table::findBucket(std::u8string_view s)
{
	// hash the string to find the primary bucket
	std::size_t stringHash = std::hash<std::u8string_view>{}(s);
	std::uint32_t &primaryBucket = (*m_primaryMapBuckets)[stringHash % m_primaryMapBuckets->size()];

	// local function to determine if this ID is the target
	const auto isTargetString = [this, &s](std::uint32_t candidateId)
	{
		return (size_t)candidateId + s.size() + 1 <= m_data.size()
			&& !memcmp(s.data(), &m_data[candidateId], s.size())
			&& m_data[candidateId + s.size()] == '\0';
	};

	SecondaryMapBucket *targetSecondaryBucket = nullptr;
	std::uint32_t *targetBucket = nullptr;
	if ((primaryBucket & 0xC0000000) == 0xC0000000)
	{
		// this is a secondary bucket
		SecondaryMapBucket &secondaryBucket = m_secondaryMapBuckets[primaryBucket & ~0xC0000000];

		// find the target
		auto iter = std::ranges::find_if(secondaryBucket, isTargetString);
		if (iter != secondaryBucket.end())
			targetBucket = &*iter;
		else
			targetSecondaryBucket = &secondaryBucket;
	}
	else if (primaryBucket == 0 || isTargetString(primaryBucket))
	{
		// the target is this primary bucket
		targetBucket = &primaryBucket;
	}
	else
	{
		// we need to change a primary bucket to a secondary bucket
		std::uint32_t newPrimaryBucketValue = 0xC0000000 | to_uint32(m_secondaryMapBuckets.size());

		// add the secondary bucket
		targetSecondaryBucket = &*m_secondaryMapBuckets.emplace(m_secondaryMapBuckets.end());

		// perform the move
		targetSecondaryBucket->push_front(primaryBucket);
		primaryBucket = newPrimaryBucketValue;
	}

	// do we need to add to the secondary bucket?
	if (targetSecondaryBucket)
	{
		targetSecondaryBucket->push_front(0);
		targetBucket = &targetSecondaryBucket->front();
	}

	// and return
	assert(targetBucket);
	return *targetBucket;
}


//-------------------------------------------------
//  string_table::internalGet
//-------------------------------------------------

std::uint32_t info::database_builder::string_table::internalGet(std::span<const char8_t> string)
{
	// sanity check - we expect string to be a NUL-terminated string
	assert(std::find(string.begin(), string.end(), '\0') - string.begin() + 1 == string.size());

	// with that out of the way, set up a string view
	std::u8string_view stringView(string.data(), string.size() - 1);

	// try encoding as a small string
	std::uint32_t result;
	std::optional<std::uint32_t> ssoResult = info::database::tryEncodeAsSmallString(stringView);
	if (ssoResult)
	{
		// it was a small string!
		result = *ssoResult;
	}
	else
	{
		// find the bucket
		std::uint32_t &bucket = findBucket(stringView);

		// did we find it?
		if (bucket == 0)
		{
			// we're going to append the string; the current size becomes the position of the new string
			bucket = to_uint32(m_data.size());

			// append the string to m_data (but keep track of where we are)
			m_data.insert(m_data.end(), string.begin(), string.end());
		}

		result = bucket;
	}

	// and return
	return result;
}


//-------------------------------------------------
//  string_table::get(const char8_t *s)
//-------------------------------------------------

std::uint32_t info::database_builder::string_table::get(const char8_t *string) noexcept
{
	std::span<const char8_t> stringSpan(string, strlen((const char *)string) + 1);
	return internalGet(stringSpan);
}


//-------------------------------------------------
//  string_table::get(const std::u8string &string)
//-------------------------------------------------

std::uint32_t info::database_builder::string_table::get(const std::u8string &string) noexcept
{
	std::span<const char8_t> stringSpan(string.data(), string.size() + 1);
	return internalGet(stringSpan);
}


//-------------------------------------------------
//  string_table::get(const XmlParser::Attribute &attribute)
//-------------------------------------------------

std::uint32_t info::database_builder::string_table::get(const XmlParser::Attribute &attribute) noexcept
{
	std::optional<const char8_t *> attributeValue = attribute.as<const char8_t *>();
	return attributeValue
		? get(*attributeValue)
		: ~0;
}


//-------------------------------------------------
//  string_table::data
//-------------------------------------------------

std::span<const char8_t> info::database_builder::string_table::data() const noexcept
{
	return m_data;
}


//-------------------------------------------------
//  string_table::lookup
//-------------------------------------------------

const char8_t *info::database_builder::string_table::lookup(std::uint32_t value, SsoBuffer &ssoBuffer) const noexcept
{
	const char8_t *result;

	std::optional<SsoBuffer> sso = info::database::tryDecodeAsSmallString(value);
	if (sso)
	{
		ssoBuffer = std::move(*sso);
		result = &ssoBuffer[0];
	}
	else
	{
		assert(value < m_data.size());
		assert(value + strlen((const char *) &m_data[value]) < m_data.size());
		result = &m_data[value];
	}
	return result;
}


//-------------------------------------------------
//  string_table::embed_value
//-------------------------------------------------

template<typename T>
void info::database_builder::string_table::embed_value(T value) noexcept
{
	const std::uint8_t *bytes = (const std::uint8_t *)&value;
	m_data.insert(m_data.end(), &bytes[0], &bytes[0] + sizeof(value));
}


//-------------------------------------------------
//  string_table::dumpStringSizeDistribution
//-------------------------------------------------

void info::database_builder::string_table::dumpStringSizeDistribution() const noexcept
{
	// list of buckets for listing distribution
	static const std::array s_buckets = { 5, 10, 20, 50, 100 };

	// array to store the size distribution
	std::array<int, std::size(s_buckets) + 1> sizeDistribution;
	std::ranges::fill(sizeDistribution, 0);

	// count the size of strings and bucket the sizes
	int totalStringCount = 0;
	auto iter = m_data.begin() + sizeof(info::binaries::MAGIC_STRINGTABLE_BEGIN);
	while (iter < m_data.end() - sizeof(info::binaries::MAGIC_STRINGTABLE_END))
	{
		auto nextIter = std::find(iter, m_data.end(), '\0');
		if (nextIter < m_data.end())
		{
			// how big is this string?
			int thisStringSize = (int)(nextIter - iter);

			// which bucket is this in?
			int thisStringSizeBucket = std::ranges::lower_bound(s_buckets, thisStringSize) - s_buckets.begin();

			// add to this bucket
			sizeDistribution[thisStringSizeBucket]++;
			totalStringCount++;

			// skip over NUL
			nextIter++;
		}
		iter = nextIter;
	}

	printf("\nDistribution of string sizes:\n");
	int rangeStart = 0;
	for (auto i = 0; i < std::size(sizeDistribution); i++)
	{
		int rangeEnd = i < std::size(s_buckets)
			? s_buckets[i]
			: -1;

		if (rangeEnd >= 0)
			printf("%3d - %3d: ", rangeStart, rangeEnd);
		else
			printf("%3d -    : ", rangeStart);
		printf("%6d (%2d%%)\n", sizeDistribution[i], sizeDistribution[i] * 100 / totalStringCount);

		rangeStart = rangeEnd + 1;
	}
	printf("  Total:   %6d\n", totalStringCount);
}


//-------------------------------------------------
//  string_table::dumpPrimaryBucketDistribution
//-------------------------------------------------

void info::database_builder::string_table::dumpPrimaryBucketDistribution() const noexcept
{
	int singularBucketCount = 0;
	int multiBucketCount = 0;
	int totalBucketCount = (int)m_primaryMapBuckets->size();

	for (std::uint32_t bucket : *m_primaryMapBuckets)
	{
		if ((bucket & 0xC0000000) == 0xC0000000)
			multiBucketCount++;
		else if (bucket > 0)
			singularBucketCount++;
	}
	int emptyBucketCount = totalBucketCount - singularBucketCount - multiBucketCount;

	printf("\nPrimary bucket distribution:\n");
	printf("%8d empty buckets    (%2d%%)\n", emptyBucketCount, emptyBucketCount * 100 / totalBucketCount);
	printf("%8d singular buckets (%2d%%)\n", singularBucketCount, singularBucketCount * 100 / totalBucketCount);
	printf("%8d multi buckets    (%2d%%)\n", multiBucketCount, multiBucketCount * 100 / totalBucketCount);
	printf("%8d total buckets\n", totalBucketCount);
}


//-------------------------------------------------
//  string_table::dumpSecondaryBucketDistribution
//-------------------------------------------------

void info::database_builder::string_table::dumpSecondaryBucketDistribution() const noexcept
{
	int total = 0;
	std::map<int, int> bucketSizeCounts;

	for (const SecondaryMapBucket &bucket : m_secondaryMapBuckets)
	{
		int bucketSize = (int)std::ranges::count_if(bucket, [](const auto &) { return true; });
		auto iter = bucketSizeCounts.find(bucketSize);
		if (iter == bucketSizeCounts.end())
			iter = bucketSizeCounts.emplace(bucketSize, 0).first;
		iter->second++;
		total++;
	}

	printf("\nSecondary bucket size distribution (%d total):\n", (int)m_secondaryMapBuckets.size());
	for (const auto &[size, count] : bucketSizeCounts)
		printf("%5d: %7d (%3d%%)\n", size, count, count * 100 / total);
}
