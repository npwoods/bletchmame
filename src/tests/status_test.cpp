/***************************************************************************

    status_test.cpp

    Unit tests for status.cpp

***************************************************************************/

#include <QBuffer>

#include "status.h"
#include "test.h"

namespace
{
    class Test : public QObject
    {
        Q_OBJECT

    private slots:
        void statusUpdateRead_mame0226()    { statusUpdateRead(":/resources/status_mame0226_coco2b_1.xml"); }
        void statusUpdateRead_mame0227()    { statusUpdateRead(":/resources/status_mame0227_coco2b_1.xml"); }
        void statusUpdateReadError_1()      { statusUpdateReadError(""); }
        void statusUpdateReadError_2()      { statusUpdateReadError("<bogusxml/>"); }

    private:
        void statusUpdateRead(const char *resourceName);
        void statusUpdateReadError(const char *bogusXmlString);
    };
};


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  statusUpdateRead
//-------------------------------------------------

void Test::statusUpdateRead(const char *resourceName)
{
    QFile statusUpdateFile(resourceName);
    QVERIFY(statusUpdateFile.open(QIODevice::ReadOnly));
    status::update statusUpdate = status::update::read(statusUpdateFile);   
    QVERIFY(statusUpdate.m_success);
    QVERIFY(statusUpdate.m_parse_error.isEmpty());
}


//-------------------------------------------------
//  statusUpdateReadError
//-------------------------------------------------

void Test::statusUpdateReadError(const char *bogusXmlString)
{
    QByteArray bogusXmlBytes = QString(bogusXmlString).toUtf8();
    QBuffer statusUpdateFile(&bogusXmlBytes);
    QVERIFY(statusUpdateFile.open(QIODevice::ReadOnly));
    status::update statusUpdate = status::update::read(statusUpdateFile);
    QVERIFY(!statusUpdate.m_success);
    QVERIFY(!statusUpdate.m_parse_error.isEmpty());
}


static TestFixture<Test> fixture;
#include "status_test.moc"
