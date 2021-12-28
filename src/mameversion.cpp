/***************************************************************************

    mameversion.cpp

    Logic for parsing and comparing MAME version strings

	Examples of MAME version strings:

		'0.238 (mame0238)'
		'0.238 (mame0238-390-g1d2d28e6e44-dirty)'

***************************************************************************/

#include "mameversion.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif // _MSC_VER


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#if defined(Q_OS_WIN32)
// -attach_window support on Win32
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_WINDOW = SimpleMameVersion(0, 213);
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW = SimpleMameVersion(0, 218);
#elif defined(Q_OS_UNIX)
// -attach_window support on Unix
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_WINDOW = SimpleMameVersion(0, 232);
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW = SimpleMameVersion(0, 232);
#else
// -attach_window support elsewhere
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_WINDOW = std::nullopt;
const std::optional<SimpleMameVersion> MameVersion::Capabilities::HAS_ATTACH_CHILD_WINDOW = std::nullopt;
#endif

// minimum MAME version
const SimpleMameVersion MameVersion::Capabilities::MINIMUM_SUPPORTED = SimpleMameVersion(0, 213);

// recording movies by specifying absolute paths was introduced in MAME 0.221
const SimpleMameVersion MameVersion::Capabilities::HAS_TOGGLE_MOVIE = SimpleMameVersion(0, 221);


//**************************************************************************
//  SIMPLEMAMEVERSION IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  SimpleMameVersion::operator MameVersion
//-------------------------------------------------

SimpleMameVersion::operator MameVersion() const
{
	return MameVersion(m_major, m_minor);
}


//-------------------------------------------------
//  SimpleMameVersion::toString
//-------------------------------------------------

QString SimpleMameVersion::toString() const
{
	// this is guaranteed to return the original string passed to us
	return QString("%1.%2 (mame%1%2)").arg(QString::number(m_major), QString::number(m_minor));
}


//-------------------------------------------------
//  SimpleMameVersion::toPrettyString - return a
//  "pretty" string for showing to end users
//-------------------------------------------------

QString SimpleMameVersion::toPrettyString() const
{
	return QString("MAME %1.%2").arg(QString::number(m_major), QString::number(m_minor));
}


//**************************************************************************
//  MAMEVERSION IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  MameVersion ctor
//-------------------------------------------------

MameVersion::MameVersion(int major, int minor)
	: m_major(major)
	, m_minor(minor)
{
}


//-------------------------------------------------
//  MameVersion ctor
//-------------------------------------------------

MameVersion::MameVersion(const QString &versionString)
{
	// this ctor parses the version - we don't have a distinct "parse()" method
	// because this is guaranteed to be lossless
	bool isDirty;
	if (sscanf(versionString.toStdString().c_str(), "%d.%d", &m_major, &m_minor) != 2)
	{
		// can't parse the string - this is a very degenerate scenario but we have
		// to handle it anyways
		m_major = 0;
		m_minor = 0;
		isDirty = true;
	}
	else
	{
		// we parsed the string; now check if we are dirty
		isDirty = versionString != MameVersion(m_major, m_minor).toString();
	}

	// if we are a dirty version, capture the raw version
	if (isDirty)
		m_rawVersionString = versionString;
}


//-------------------------------------------------
//  MameVersion::isAtLeast
//-------------------------------------------------

bool MameVersion::isAtLeast(const SimpleMameVersion &that) const
{
	bool result;
	if (major() > that.major())
		result = true;
	else if (major() < that.major())
		result = false;
	else if (minor() >= that.minor())
		result = true;
	else
		result = false;
	return result;
}


//-------------------------------------------------
//  MameVersion::nextCleanVersion
//-------------------------------------------------

SimpleMameVersion MameVersion::nextCleanVersion() const
{
	return SimpleMameVersion(major(), minor() + (isDirty() ? 1 : 0));
}


//-------------------------------------------------
//  MameVersion::toString
//-------------------------------------------------

QString MameVersion::toString() const
{
	// this is guaranteed to return the original string passed to us
	return m_rawVersionString.has_value()
		? m_rawVersionString.value()
		: toSimpleMameVersion().toString();
}


//-------------------------------------------------
//  MameVersion::toPrettyString - return a "pretty"
//  string for showing to end users
//-------------------------------------------------

QString MameVersion::toPrettyString() const
{
	QString result;
	if (!m_rawVersionString.has_value())
	{
		// normal case, we're using a real MAME release
		result = toSimpleMameVersion().toPrettyString();
	}
	else if (!m_rawVersionString->isEmpty())
	{
		// we're using a dirty MAME version
		result = QString("MAME %1").arg(m_rawVersionString.value());
	}
	else
	{
		// something very weird is going on
		result = "MAME ???";
	}
	return result;
}



//-------------------------------------------------
//  MameVersion::toSimpleMameVersion
//-------------------------------------------------

SimpleMameVersion MameVersion::toSimpleMameVersion() const
{
	assert(!m_rawVersionString.has_value());
	return SimpleMameVersion(major(), minor());
}
