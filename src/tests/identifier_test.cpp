/***************************************************************************

	identifier_test.cpp

	Unit tests for identifier.cpp

***************************************************************************/

// bletchmame headers
#include "identifier.h"
#include "test.h"


namespace
{
	// ======================> Test

	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void identifierHash();
		void identifierEqualTo();
	};
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  identifierHash
//-------------------------------------------------

void Test::identifierHash()
{
	using namespace std::literals;

	Identifier ident1 = MachineIdentifier(u8"blah"sv);
	Identifier ident2 = MachineIdentifier(u8"blah"sv);
	QVERIFY(std::hash<Identifier>()(ident1) == std::hash<Identifier>()(ident2));

	Identifier ident3 = SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv);
	Identifier ident4 = SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv);
	QVERIFY(std::hash<Identifier>()(ident3) == std::hash<Identifier>()(ident4));
}


//-------------------------------------------------
//  identifierEqualTo
//-------------------------------------------------

void Test::identifierEqualTo()
{
	using namespace std::literals;

	QVERIFY(std::equal_to<Identifier>()(
		MachineIdentifier(u8"alpha"sv),
		MachineIdentifier(u8"alpha"sv)));

	QVERIFY(!std::equal_to<Identifier>()(
		MachineIdentifier(u8"alpha"sv),
		MachineIdentifier(u8"bravo"sv)));

	QVERIFY(std::equal_to<Identifier>()(
		SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv),
		SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv)));

	QVERIFY(!std::equal_to<Identifier>()(
		SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv),
		SoftwareIdentifier(u8"charlie"sv, u8"delta"sv)));

	QVERIFY(!std::equal_to<Identifier>()(
		SoftwareIdentifier(u8"alpha"sv, u8"bravo"sv),
		MachineIdentifier(u8"charlie"sv)));
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "identifier_test.moc"
