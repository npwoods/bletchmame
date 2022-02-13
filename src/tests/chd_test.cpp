/***************************************************************************

	chd_test.cpp

	Unit tests for chd.cpp

***************************************************************************/

// bletchmame headers
#include "chd.h"
#include "test.h"

namespace
{
	static std::array<std::uint8_t, 20> s_sampleChdHash = { 0xBF, 0xEC, 0x48, 0xAE, 0x24, 0x39, 0x30, 0x8A, 0xC3, 0xA5, 0x47, 0x23, 0x1A, 0x13, 0xF1, 0x22, 0xEF, 0x30, 0x3C, 0x76 };

	class Test : public QObject
	{
		Q_OBJECT

	private slots:
		void getHashForChd_0()	{ getHashForChd(":/resources/garbage.bin", { }); }
		void getHashForChd_1()	{ getHashForChd(":/resources/samplechd.chd", Hash(s_sampleChdHash)); }

	private:
		void getHashForChd(const QString &fileName, const std::optional<Hash> &expectedHash);
	};
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getHashForChd
//-------------------------------------------------

void Test::getHashForChd(const QString &fileName, const std::optional<Hash> &expectedHash)
{
	// open the file
	QFile file(fileName);
	QVERIFY(file.open(QIODevice::ReadOnly));

	// get the hash for this file
	std::optional<Hash> actualHash = ::getHashForChd(file);

	// and validate
	QVERIFY(actualHash == expectedHash);
}


//-------------------------------------------------

static TestFixture<Test> fixture;
#include "chd_test.moc"
