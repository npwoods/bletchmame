/***************************************************************************

	profile.cpp

	Code for representing profiles

***************************************************************************/

#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <optional>

#include "profile.h"
#include "xmlparser.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  image::operator==
//-------------------------------------------------

bool profiles::image::operator==(const image &that) const
{
	return m_tag == that.m_tag
		&& m_path == that.m_path;
}


//-------------------------------------------------
//  image::is_valid
//-------------------------------------------------

bool profiles::image::is_valid() const
{
	return !m_tag.isEmpty() && !m_path.isEmpty();
}


//-------------------------------------------------
//  ctor
//-------------------------------------------------

profiles::profile::profile()
{
}


//-------------------------------------------------
//  operator==
//-------------------------------------------------

bool profiles::profile::operator==(const profile &that) const
{
	return m_name == that.m_name
		&& m_software == that.m_software
		&& m_path == that.m_path
		&& m_images == that.m_images;
}


//-------------------------------------------------
//  is_valid
//-------------------------------------------------

bool profiles::profile::is_valid() const
{
	return !m_name.isEmpty()
		&& !m_path.isEmpty()
		&& !m_machine.isEmpty()
		&& std::find_if(m_images.begin(), m_images.end(), [](const image &x) { return !x.is_valid(); }) == m_images.end();
}


//-------------------------------------------------
//  scan_directory
//-------------------------------------------------

std::vector<profiles::profile> profiles::profile::scan_directories(const QStringList &paths)
{
	std::vector<profile> results;

	for (const QString &path : paths)
	{
		QDirIterator iter(path, QStringList() << "*.bletchmameprofile");
		while (iter.hasNext())
		{
			QString file = iter.next();
			std::optional<profile> p = load(std::move(file));
			if (p)
				results.push_back(std::move(p.value()));
		}
	}
	return results;
}


//-------------------------------------------------
//  load
//-------------------------------------------------

std::optional<profiles::profile> profiles::profile::load(const QString &path)
{
	return load(QString(path));
}


//-------------------------------------------------
//  load
//-------------------------------------------------

std::optional<profiles::profile> profiles::profile::load(QString &&path)
{
	// open the stream
	QFile file(path);
	if (!file.open(QFile::ReadOnly))
		return { };
	QDataStream stream(&file);

	// start setting up the profile
	profile result;
	result.m_path = std::move(path);
	QFileInfo fi(file);
	result.m_name = fi.baseName();

	// and parse the XML
	XmlParser xml;
	xml.OnElementBegin({ "profile" }, [&](const XmlParser::Attributes &attributes)
	{
		attributes.Get("machine", result.m_machine);
		attributes.Get("software", result.m_software);
	});
	xml.OnElementBegin({ "profile", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &i = result.m_images.emplace_back();
		attributes.Get("tag",	i.m_tag);
		attributes.Get("path",	i.m_path);
	});

	return xml.Parse(result.m_path) && result.is_valid()
		? std::optional<profiles::profile>(std::move(result))
		: std::optional<profiles::profile>();
}


//-------------------------------------------------
//  save
//-------------------------------------------------

void profiles::profile::save() const
{
	QFile file(path());
	if (file.open(QFile::WriteOnly))
	{
		QTextStream stream(&file);
		save_as(stream);
	}
}


//-------------------------------------------------
//  save_as
//-------------------------------------------------

void profiles::profile::save_as(QTextStream &stream) const
{
	stream << "<!-- BletchMAME profile -->" << Qt::endl;
	stream << "<profile";
	stream << " machine=\"" << machine() << "\"";
	if (!software().isEmpty())
		stream << " software=\"" << software() << "\"";
	stream << ">" << Qt::endl;

	for (const image &image : images())
		stream << "\t<image tag=\"" << image.m_tag << "\" path=\"" << image.m_path << "\"/>" << Qt::endl;
	stream << "</profile>" << Qt::endl;
}


//-------------------------------------------------
//  create
//-------------------------------------------------

void profiles::profile::create(QTextStream &stream, const info::machine &machine, const software_list::software *software)
{
	profile new_profile;
	new_profile.m_machine = machine.name();
	if (software)
		new_profile.m_software = software->m_name;
	new_profile.save_as(stream);
}


//**************************************************************************
//  UTILITY
//**************************************************************************

//-------------------------------------------------
//  change_path_save_state
//-------------------------------------------------

QString profiles::profile::change_path_save_state(const QString &path)
{
	QFileInfo fi(path);
	return fi.path() + "/" + fi.completeBaseName() + ".sta";
}


//-------------------------------------------------
//  profile_file_rename
//-------------------------------------------------

bool profiles::profile::profile_file_rename(const QString &old_path, const QString &new_path)
{
	bool success;

	if (new_path == old_path)
	{
		// if the new path is equal to the old path, renaming is easy - its a no-op
		success = true;
	}
	else
	{
		// this is a crude multi file renaming; unfortunately there is not a robust way to do this
		success = QFile::rename(old_path, new_path);
		if (success)
		{
			QString old_save_state_path = change_path_save_state(old_path);
			QString new_save_state_path = change_path_save_state(new_path);
			QFileInfo fi(old_save_state_path);
			if (fi.exists(old_save_state_path))
				QFile::rename(old_save_state_path, new_save_state_path);
		}
	}
	return success;
}


//-------------------------------------------------
//  profile_file_remove
//-------------------------------------------------

bool profiles::profile::profile_file_remove(const QString &path)
{
	// this is also crude
	bool success = QFile::remove(path);
	if (success)
	{
		QString save_state_path = change_path_save_state(path);
		QFileInfo save_state_fi(save_state_path);
		if (save_state_fi.exists())
			QFile::remove(save_state_path);
	}
	return success;
}
