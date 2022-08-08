/***************************************************************************

	identifier.h

	Implements identifiers for machines or software

***************************************************************************/

#pragma once

#ifndef IDENTIFIER_H
#define IDENTIFIER_H

// Qt headers
#include <QString>

// standard headers
#include <string>
#include <variant>


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

// ======================> MachineIdentifier

class MachineIdentifier
{
public:
	// ctor
	MachineIdentifier(std::u8string &&machineName);
	MachineIdentifier(std::u8string_view machineName);
	MachineIdentifier(const QString &machineName);
	MachineIdentifier(const MachineIdentifier &) = default;
	MachineIdentifier(MachineIdentifier &&) = default;

	// operators
	MachineIdentifier &operator=(const MachineIdentifier &) = default;
	bool operator==(const MachineIdentifier &) const = default;

	// accessors
	const std::u8string &machineName() const { return m_machineName; }

private:
	std::u8string	m_machineName;
};


namespace std
{
	template<>
	struct hash<MachineIdentifier>
	{
		std::size_t operator()(const MachineIdentifier &x) const;
	};
}


// ======================> SoftwareIdentifier

class SoftwareIdentifier
{
public:
	friend struct std::hash<SoftwareIdentifier>;

	// ctor
	SoftwareIdentifier(std::u8string_view softwareList, std::u8string_view software);
	SoftwareIdentifier(const QString &softwareList, const QString &software);
	SoftwareIdentifier(const SoftwareIdentifier &) = default;
	SoftwareIdentifier(SoftwareIdentifier &&) = default;

	// operators
	SoftwareIdentifier &operator=(const SoftwareIdentifier &) = default;
	bool operator==(const SoftwareIdentifier &) const = default;

	// accessors
	std::u8string_view softwareList() const;
	std::u8string_view software() const;

private:
	std::u8string	m_text;
	std::size_t		m_listLength;
};


namespace std
{
	template<>
	struct hash<SoftwareIdentifier>
	{
		std::size_t operator()(const SoftwareIdentifier &x) const;
	};
}


// ======================> Identifier

typedef std::variant<MachineIdentifier, SoftwareIdentifier> Identifier;

#endif // IDENTIFIER_H
