/***************************************************************************

    buildversion.cpp

    Encapsulating access for BletchMAME version strings

***************************************************************************/

#include "buildversion.h"


//**************************************************************************
//  VERSION INFO
//**************************************************************************

#if HAS_BUILDVERSION_GEN_CPP
#include "buildversion.gen.cpp"
const std::optional<BuildVersion> BuildVersion::s_instance = BuildVersion(buildVersion, buildRevision, buildDateTime);
#else
const std::optional<BuildVersion> BuildVersion::s_instance;
#endif


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

BuildVersion::BuildVersion(const char *version, const char *revision, const char *dateTime)
    : m_version(version)
    , m_revision(revision)
    , m_dateTime(dateTime)
{
}
