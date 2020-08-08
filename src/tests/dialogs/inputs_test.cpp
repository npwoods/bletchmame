/***************************************************************************

	dialogs/inputs_test.cpp

	Unit tests for dialogs/inputs.cpp

***************************************************************************/

#include "dialogs/inputs.h"
#include "../test.h"

class InputsDialog::Test : public QObject
{
	Q_OBJECT

private slots:
	void getSeqTextFromTokens();
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getSeqTextFromTokens
//-------------------------------------------------

void InputsDialog::Test::getSeqTextFromTokens()
{
	// bogus codes map
	const std::unordered_map<QString, QString> codes =
	{
		{ "KEYCODE_A",			"A" },
		{ "KEYCODE_B",			"B" },
		{ "KEYCODE_C",			"C" },
		{ "JOYCODE_1_XAXIS",	"Joy #1 X Axis" }
	};

	auto test = [&codes](const QString &tokens, const QString &expectedResult)
	{
		QString actualResult = InputsDialog::GetSeqTextFromTokens("", codes);
		QVERIFY(actualResult == expectedResult);
	};

	// test cases
	test("",						"");
	test("KEYCODE_A",				"A");
	test("KEYCODE_B",				"B");
	test("KEYCODE_B OR KEYCODE_C",	"B or C");
	test("JOYCODE_1_XAXIS",			"Joy #1 X Axis");
}


static TestFixture<InputsDialog::Test> fixture;
#include "inputs_test.moc"
