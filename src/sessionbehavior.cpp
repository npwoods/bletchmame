/***************************************************************************

    sessionbehavior.cpp

    Abstracting runtime behaviors of the emulation (e.g. - profiles)

***************************************************************************/

// bletchmame headers
#include "sessionbehavior.h"


//**************************************************************************
//  SESSIONBEHAVIOR
//**************************************************************************

//-------------------------------------------------
//  SessionBehavior dtor
//-------------------------------------------------

SessionBehavior::~SessionBehavior()
{
}


//**************************************************************************
//  NORMALSESSIONBEHAVIOR
//**************************************************************************

//-------------------------------------------------
//  NormalSessionBehavior ctor
//-------------------------------------------------

NormalSessionBehavior::NormalSessionBehavior(const software_list::software *software)
{
	if (software)
		m_initialSoftware = QString("%1:%2").arg(software->parent().name(), software->name());
}


//-------------------------------------------------
//  NormalSessionBehavior::getOptions
//-------------------------------------------------

QString NormalSessionBehavior::getInitialSoftware() const
{
    return m_initialSoftware;
}


//-------------------------------------------------
//  NormalSessionBehavior::getOptions
//-------------------------------------------------

std::map<QString, QString> NormalSessionBehavior::getOptions() const
{
    return { };
}


//-------------------------------------------------
//  NormalSessionBehavior::getOptions
//-------------------------------------------------

std::map<QString, QString> NormalSessionBehavior::getImages() const
{
    return { };
}


//-------------------------------------------------
//  NormalSessionBehavior::getSavedState
//-------------------------------------------------

QString NormalSessionBehavior::getSavedState() const
{
    return { };
}


//-------------------------------------------------
//  NormalSessionBehavior::persistState
//-------------------------------------------------

void NormalSessionBehavior::persistState(const std::vector<status::slot> &devslots, const std::vector<status::image> &images)
{
}


//-------------------------------------------------
//  NormalSessionBehavior::shouldPromptOnStop
//-------------------------------------------------

bool NormalSessionBehavior::shouldPromptOnStop() const
{
    return true;
}


//**************************************************************************
//  PROFILESESSIONBEHAVIOR
//**************************************************************************

//-------------------------------------------------
//  ProfileSessionBehavior ctor
//-------------------------------------------------

ProfileSessionBehavior::ProfileSessionBehavior(std::shared_ptr<profiles::profile> &&profile)
    : m_profile(std::move(profile))
{
}


//-------------------------------------------------
//  ProfileSessionBehavior::getOptions
//-------------------------------------------------

QString ProfileSessionBehavior::getInitialSoftware() const
{
    return m_profile->software();
}


//-------------------------------------------------
//  ProfileSessionBehavior::getOptions
//-------------------------------------------------

std::map<QString, QString> ProfileSessionBehavior::getOptions() const
{
    std::map<QString, QString> result;
    for (const auto &slot : m_profile->devslots())
        result.insert({ slot.m_name, slot.m_value });
    return result;
}


//-------------------------------------------------
//  ProfileSessionBehavior::getOptions
//-------------------------------------------------

std::map<QString, QString> ProfileSessionBehavior::getImages() const
{
    std::map<QString, QString> result;
    for (const auto &image : m_profile->images())
        result.insert({ image.m_tag, image.m_path });
    return result;
}


//-------------------------------------------------
//  ProfileSessionBehavior::getSavedState
//-------------------------------------------------

QString ProfileSessionBehavior::getSavedState() const
{
    return m_profile->auto_save_states()
        ? profiles::profile::change_path_save_state(m_profile->path())
        : QString();
}


//-------------------------------------------------
//  ProfileSessionBehavior::persistState
//-------------------------------------------------

void ProfileSessionBehavior::persistState(const std::vector<status::slot> &devslots, const std::vector<status::image> &images)
{
    // update slots
    m_profile->devslots().clear();
    for (const status::slot &status_slot : devslots)
    {
        profiles::slot &profile_slot = m_profile->devslots().emplace_back();
        profile_slot.m_name = status_slot.m_name;
        profile_slot.m_value = status_slot.m_current_option;
    }

    // update images
	m_profile->images().clear();
	for (const status::image &status_image : images)
	{
		if (!status_image.m_file_name.isEmpty())
		{
			profiles::image &profile_image = m_profile->images().emplace_back();
			profile_image.m_tag = status_image.m_tag;
			profile_image.m_path = status_image.m_file_name;
		}
	}

    // clear out the software; we've captured this state with the images
    m_profile->setSoftware("");

    // and save the profile
    m_profile->save();
}


//-------------------------------------------------
//  ProfileSessionBehavior::shouldPromptOnStop
//-------------------------------------------------

bool ProfileSessionBehavior::shouldPromptOnStop() const
{
    return !m_profile->auto_save_states();
}
