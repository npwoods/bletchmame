/***************************************************************************

	profile.h

	Code for representing profiles

***************************************************************************/

#pragma once

#ifndef PROFILE_H
#define PROFILE_H

#include <vector>

#include "info.h"
#include "softwarelist.h"

QT_BEGIN_NAMESPACE
class QTextStream;
QT_END_NAMESPACE

namespace profiles
{
	struct image
	{
		QString		m_tag;
		QString		m_path;

		bool operator==(const image &that) const;
		bool is_valid() const;
	};

	class profile
	{
	public:
		profile(profile &&) = default;
		profile &operator =(profile &&) = default;

		// accessors
		const QString &name() const					{ return m_name; }
		const QString &path() const					{ return m_path; }
		const QString &machine() const				{ return m_machine; }
		const QString &software() const				{ return m_software; }
		const std::vector<image> &images() const	{ return m_images; }
		std::vector<image> &images()				{ return m_images; }
		bool auto_save_states() const				{ return true; }
		
		// methods
		bool is_valid() const;
		void save() const;
		void save_as(QTextStream &stream) const;

		QString directory_path() const
		{
			QString result;
			wxFileName::SplitPath(path(), &result, nullptr, nullptr);
			return result;
		}

		// statics
		static std::vector<profile> scan_directories(const QStringList &paths);
		static void create(QTextStream &stream, const info::machine &machine, const software_list::software *software);
		static std::optional<profile> load(QString &&path);
		static std::optional<profile> load(const QString &path);

		// utility
		static QString change_path_save_state(const QString &path);
		static bool profile_file_rename(const QString &old_path, const QString &new_path);
		static bool profile_file_remove(const QString &path);

		bool operator==(const profile &that) const;

	private:
		profile();
		profile(const profile &) = delete;
		profile &operator =(const profile &) = delete;

		QString				m_name;
		QString				m_path;
		QString				m_machine;
		QString				m_software;
		std::vector<image>	m_images;
	};
};

#endif // PROFILE_H
