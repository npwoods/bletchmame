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

std::vector<std::shared_ptr<profiles::profile>> profiles::profile::scan_directories(const QStringList &paths)
{
	std::vector<std::shared_ptr<profiles::profile>> results;

	for (const QString &path : paths)
	{
		QDirIterator iter(path, QStringList() << "*.bletchmameprofile");
		while (iter.hasNext())
		{
			QString file = iter.next();
			std::optional<profile> profile = load(std::move(file));
			if (profile)
			{
				std::shared_ptr<profiles::profile> profilePtr = std::make_shared<profiles::profile>(std::move(profile.value()));
				results.push_back(std::move(profilePtr));
			}
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

	QFileInfo fi(file);
	return load(file, std::move(path), fi.baseName());
}


//-------------------------------------------------
//  load
//-------------------------------------------------

std::optional<profiles::profile> profiles::profile::load(QIODevice & stream, QString &&path, QString &&name)
{
	// start setting up the profile
	profile result;
	result.m_path = std::move(path);
	result.m_name = std::move(name);

	// and parse the XML
	XmlParser xml;
	xml.onElementBegin({ "profile" }, [&](const XmlParser::Attributes &attributes)
	{
		result.m_machine	= attributes.get<QString>("machine").value_or("");
		result.m_software	= attributes.get<QString>("software").value_or("");
	});
	xml.onElementBegin({ "profile", "image" }, [&](const XmlParser::Attributes &attributes)
	{
		image &i = result.m_images.emplace_back();
		i.m_tag				= attributes.get<QString>("tag").value_or("");
		i.m_path			= attributes.get<QString>("path").value_or("");
	});
	xml.onElementBegin({ "profile", "slot" }, [&](const XmlParser::Attributes &attributes)
	{
		slot &s = result.m_slots.emplace_back();
		s.m_name			= attributes.get<QString>("name").value_or("");
		s.m_value			= attributes.get<QString>("value").value_or("");
	});

	return xml.parse(stream) && result.is_valid()
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
		save_as(file);
}


//-------------------------------------------------
//  save_as
//-------------------------------------------------

void profiles::profile::save_as(QIODevice &stream) const
{
	QTextStream textStream(&stream);

	textStream << "<!-- BletchMAME profile -->" << Qt::endl;
	textStream << "<profile";
	textStream << " machine=\"" << machine() << "\"";
	if (!software().isEmpty())
		textStream << " software=\"" << software() << "\"";
	textStream << ">" << Qt::endl;

	for (const slot &slot : devslots())
		textStream << "\t<slot name=\"" << slot.m_name << "\" value=\"" << slot.m_value << "\"/>" << Qt::endl;
	for (const image &image : images())
		textStream << "\t<image tag=\"" << image.m_tag << "\" path=\"" << image.m_path << "\"/>" << Qt::endl;
	textStream << "</profile>" << Qt::endl;
}


//-------------------------------------------------
//  create
//-------------------------------------------------

void profiles::profile::create(QIODevice &stream, const info::machine &machine, const software_list::software *software)
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
