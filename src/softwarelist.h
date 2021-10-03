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

	private:
		QString				m_name;
		QString				m_interface;
	};

	class software
	{
		friend class software_list;

	public:
		// ctor
		software() = default;
		software(const software &) = delete;
		software(software &&) = default;

		// accessors
		const QString &name() const				{ return m_name; }
		const QString &description() const		{ return m_description; }
		const QString &year() const				{ return m_year; }
		const QString &publisher() const		{ return m_publisher; }
		const std::vector<part> &parts() const	{ return m_parts; }

	private:
		QString				m_name;
		QString				m_description;
		QString				m_year;
		QString				m_publisher;
		std::vector<part>	m_parts;
	};

	software_list(const software_list &) = delete;
	software_list(software_list &&) = default;

	// attempts to load a software list from a list of paths
	static std::optional<software_list> try_load(const QStringList &hash_paths, const QString &softlist_name);

	// accessors
	const QString &name() const							{ return m_name; }
	const std::vector<software> &get_software() const	{ return m_software; }

private:
	QString				m_name;
	QString				m_description;
	std::vector<software>	m_software;

	// ctor
	software_list() = default;

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
	const std::vector<software_list> &software_lists() const { return m_software_lists; }

	// methods
	void load(const Preferences &prefs, info::machine machine);
	const software_list::software *find_software_by_name(const QString &name, const QString &dev_interface) const;

private:
	std::vector<software_list>		m_software_lists;
};

#endif // SOFTWARELIST_H
