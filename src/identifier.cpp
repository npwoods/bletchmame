/***************************************************************************

	identifier.cpp

	Implements identifiers for machines or software

***************************************************************************/

// bletchmame headers
#include "identifier.h"
#include "utility.h"

// standard headers
#include <cassert>


//**************************************************************************
//  MAIN IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  MachineIdentifier ctor
//-------------------------------------------------

MachineIdentifier::MachineIdentifier(std::u8string &&machineName)
	: m_machineName(std::move(machineName))
{
}


//-------------------------------------------------
//  MachineIdentifier ctor
//-------------------------------------------------

MachineIdentifier::MachineIdentifier(std::u8string_view machineName)
	: MachineIdentifier(std::u8string(machineName))
{
}


//-------------------------------------------------
//  MachineIdentifier ctor
//-------------------------------------------------

MachineIdentifier::MachineIdentifier(const QString &machineName)
	: MachineIdentifier(util::toU8String(machineName))
{
}


//-------------------------------------------------
//  std::hash<MachineIdentifier>::operator()
//-------------------------------------------------

std::size_t std::hash<MachineIdentifier>::operator()(const MachineIdentifier &identifier) const
{
	return std::hash<std::u8string>()(identifier.machineName());
}


//-------------------------------------------------
//  SoftwareIdentifier ctor
//-------------------------------------------------

SoftwareIdentifier::SoftwareIdentifier(std::u8string_view softwareList, std::u8string_view software)
	: m_text(softwareList)
	, m_listLength(softwareList.size())
{
	m_text.append(software);
}


//-------------------------------------------------
//  SoftwareIdentifier ctor
//-------------------------------------------------

SoftwareIdentifier::SoftwareIdentifier(const QString &softwareList, const QString &software)
	: SoftwareIdentifier(util::toU8String(softwareList), util::toU8String(software))
{
}


//-------------------------------------------------
//  SoftwareIdentifier::softwareList
//-------------------------------------------------

std::u8string_view SoftwareIdentifier::softwareList() const
{
	return std::u8string_view(&m_text[0], m_listLength);
}


//-------------------------------------------------
//  SoftwareIdentifier::software
//-------------------------------------------------

std::u8string_view SoftwareIdentifier::software() const
{
	return std::u8string_view(&m_text[m_listLength], m_text.size() - m_listLength);
}


//-------------------------------------------------
//  std::hash<SoftwareIdentifier>::operator()
//-------------------------------------------------

std::size_t std::hash<SoftwareIdentifier>::operator()(const SoftwareIdentifier &identifier) const
{
	return std::hash<std::u8string>()(identifier.m_text)
		^ std::hash<std::size_t>()(identifier.m_listLength);
}