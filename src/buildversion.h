/***************************************************************************

    buildversion.h

    Encapsulating access for BletchMAME version strings

***************************************************************************/

#pragma once

#ifndef BUILDVERSION_H
#define BUILDVERSION_H

#include <optional>


class BuildVersion
{
public:
    static const std::optional<BuildVersion> s_instance;

    // accessors
    const char *version() const { return m_version; }
    const char *revision() const { return m_revision; }
    const char *dateTime() const { return m_dateTime; }

private:
    const char *m_version;
    const char *m_revision;
    const char *m_dateTime;

    BuildVersion(const char *version, const char *revision, const char *dateTime);
};

#endif // BUILDVERSION_H
