/***************************************************************************

    listxmlrunner.cpp

    Testing infrastructure for our ability to handle -listxml output

***************************************************************************/

#include <iostream>
#include <stdexcept>
#include <QBuffer>
#include <QProcess>

#include "test.h"
#include "info.h"
#include "info_builder.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  internalRunAndExcerciseListXml
//-------------------------------------------------

static void internalRunAndExcerciseListXml(const QString &program)
{
    // check to see if the program file exists
    QFileInfo fileInfo(program);
    if (!fileInfo.isFile())
        throw std::logic_error(QString("MAME program '%1' not found").arg(program).toLocal8Bit().constData());

    QByteArray infoDbBytes;
    {
        // invoke MAME with -listxml
        std::cout << "Running -listxml..." << std::endl;
        QProcess process;
        process.setReadChannel(QProcess::StandardOutput);
        process.start(program, QStringList() << "-listxml");

        // and process the output
        QString errorMessage;
        info::database_builder builder;
        QVERIFY(builder.process_xml(process, errorMessage));
        QVERIFY(errorMessage.isEmpty());

        // and put it in the byte array
        QBuffer buffer(&infoDbBytes);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        builder.emit_info(buffer);
    }

    // report our status and sanity checks
    std::cout << "Info DB build complete (database size " << infoDbBytes.size() << " bytes)" << std::endl;
    QVERIFY(infoDbBytes.size() > 0);

    // and now read it
    info::database db;
    {
        // prepare buffer for reading
        QBuffer buffer(&infoDbBytes);
        QVERIFY(buffer.open(QIODevice::ReadOnly));

        // and process it, validating we've done so successfully
        bool dbChanged = false;
        db.setOnChanged([&dbChanged]() { dbChanged = true; });
        QVERIFY(db.load(buffer));
        QVERIFY(dbChanged);
    }

    // report our status and sanity checks
    std::cout << "Info DB read complete (" << db.machines().size() << " total machines)" << std::endl;
    QVERIFY(db.machines().size() > 0);
}


//-------------------------------------------------
//  runAndExcerciseListXml - test our ability to
//  invoke and handle -listxml output
//-------------------------------------------------

int runAndExcerciseListXml(int argc, char *argv[])
{
    // identify the program
    QString program = argv[0];

    int result;
    try
    {
        internalRunAndExcerciseListXml(program);
        result = 0;
    }
    catch (std::exception &ex)
    {
        std::cout << "EXCEPTION: " << ex.what() << std::endl;
        result = 1;
    }
    return result;
}
