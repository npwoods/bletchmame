/***************************************************************************

	softwarelist.h

	Support for software lists

***************************************************************************/

#pragma once

#ifndef SOFTWARELIST_H
#define SOFTWARELIST_H

#include <QString>

#include <optional>

#include "utility.h"
#include "info.h"

class Preferences;

QT_BEGIN_NAMESPACE
class QDataStream;
QT_END_NAMESPACE


// ======================> software_list
class software_list
{
public:
	class test;
	class software;

	typedef std::unique_ptr<software_list> ptr;

	class rom
	{
		friend class software_list;

	public:
		// ctor
		rom() = default;
		rom(const rom &) = delete;
		rom(rom &&) = default;

		// accessors
		const QString &name() const										{ return m_name; }
		const std::optional<std::uint64_t> &size() const				{ return m_size; }
		const std::optional<std::uint32_t> &crc32() const				{ return m_crc32; }
		const std::optional<std::array<std::uint8_t, 20>> &sha1() const	{ return m_sha1; }

	private:
		QString										m_name;
		std::optional<std::uint64_t>				m_size;
		std::optional<std::uint32_t>				m_crc32;
		std::optional<std::array<std::uint8_t, 20>>	m_sha1;
	};

	class dataarea
	{
		friend class software_list;

	public:
		// ctor
		dataarea() = default;
		dataarea(const dataarea &) = delete;
		dataarea(dataarea &&) = default;

		// accessors
		const QString &name() const				{ return m_name; }
		const std::vector<rom> &roms() const	{ return m_roms; }

	private:
		QString					m_name;
		std::vector<rom>		m_roms;
	};

	class part
	{
		friend class software_list;

	public:
		// ctor
		part() = default;
		part(const part &) = delete;
		part(part &&) = default;
	
		// accessors
		const QString &name() const				{ return m_name; }
		const QString &interface() const		{ return m_interface; }
		const std::vector<dataarea> &dataareas() const { return m_dataareas; }

	private:
		QString					m_name;
		QString					m_interface;
		std::vector<dataarea>	m_dataareas;
	};

	class software
	{
		friend class software_list;

	public:
		// ctor/dtor
		software(const software_list &parent);
		software(const software &) = delete;
		software(software &&) = default;
		~software();

		// accessors
		const software_list &parent() const		{ return m_parent; }
		const QString &name() const				{ return m_name; }
		const QString &description() const		{ return m_description; }
		const QString &year() const				{ return m_year; }
		const QString &publisher() const		{ return m_publisher; }
		const std::vector<part> &parts() const	{ return m_parts; }

	private:
		const software_list &	m_parent;
		QString					m_name;
		QString					m_description;
		QString					m_year;
		QString					m_publisher;
		std::vector<part>		m_parts;
	};

	// ctor
	software_list() = default;
	software_list(const software_list &) = delete;
	software_list(software_list &&) = delete;

	// attempts to load a software list from a list of paths
	static software_list::ptr try_load(const QStringList &hash_paths, const QString &softlist_name);

	// accessors
	const QString &name() const							{ return m_name; }
	const std::vector<software> &get_software() const	{ return m_software; }

private:
	QString					m_name;
	QString					m_description;
	std::vector<software>	m_software;

	// methods
	bool load(QIODevice &stream, QString &error_message);
};


// ======================> software_list_collection
class software_list_collection
{
public:
	software_list_collection() = default;
	software_list_collection(const software_list_collection &) = delete;
	software_list_collection(software_list_collection &&) = default;

	// accessors
	const std::vector<software_list::ptr> &software_lists() const { return m_software_lists; }

	// methods
	void load(const Preferences &prefs, info::machine machine);
	const software_list::software *find_software_by_name(const QString &name, const QString &dev_interface) const;
	const software_list::software *find_software_by_list_and_name(const QString &softwareList, const QString &software) const;

private:
	std::vector<software_list::ptr>		m_software_lists;
};

#endif // SOFTWARELIST_H
