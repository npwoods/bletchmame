/***************************************************************************

    listxmlrunner.cpp

    Testing infrastructure for our ability to handle -listxml output

***************************************************************************/

// bletchmame headers
#include "test.h"
#include "info.h"
#include "info_builder.h"

// Qt headers
#include <QBuffer>
#include <QProcess>

// standard headers
#include <iostream>
#include <stdexcept>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

namespace
{
    class Stopwatch
    {
    public:
        Stopwatch()
        {
            _start = std::clock();
        }

        auto elapsedDuration() const
        {
            std::clock_t current = std::clock();
            return std::chrono::duration<std::clock_t, std::ratio<1, CLOCKS_PER_SEC>>(current - _start);
        }

    private:
        std::clock_t _start;
    };
}


//-------------------------------------------------
//  invokeListXml
//-------------------------------------------------

static void invokeListXml(QProcess &process, const QString &program)
{
    process.setReadChannel(QProcess::StandardOutput);
    process.start(program, QStringList() << "-listxml");
}


//-------------------------------------------------
//  toLocal8BitString
//-------------------------------------------------

static std::string toLocal8BitString(std::u8string_view s)
{
	QString qs = util::toQString(s);
	QByteArray byteArray = qs.toLocal8Bit();
	return std::string(byteArray.data(), byteArray.size());
}


//-------------------------------------------------
//  processXmlCallback
//-------------------------------------------------

static void processXmlCallback(int machineCount, std::u8string_view machineName, std::u8string_view machineDescription)
{
	std::string localMachineName = toLocal8BitString(machineName);
	std::string localMachineDescription = toLocal8BitString(machineDescription);
	
	// print this on one line
	char buffer[65];
	snprintf(buffer, std::size(buffer), "%s (%s)...", localMachineName.c_str(), localMachineDescription.c_str());
	printf("Processing %-*s\r", -((int) std::size(buffer) - 1), buffer);
}


//-------------------------------------------------
//  internalRunAndExcerciseListXml
//-------------------------------------------------

static void internalRunAndExcerciseListXml(const QString &program, bool sequential)
{
	// check to see if the program file exists
	QFileInfo fileInfo(program);
	if (!fileInfo.isFile())
		throw std::logic_error(QString("MAME program '%1' not found").arg(program).toLocal8Bit().constData());

	// are we going to run -listxml and build info DB simultaneously, or sequentially?
	std::unique_ptr<QIODevice> listXmlSource;
	QByteArray listXmlBytes;
	if (sequential)
	{
		// we're invoke MAME with -listxml as a distinct step, and then running info DB build sequentially
		Stopwatch listXmlStopwatch;
		{
			std::cout << "Running -listxml..." << std::endl;
			QProcess process;
			invokeListXml(process, program);
			process.waitForFinished();
			listXmlBytes = process.readAll();
		}
		auto listXmlDuration = std::chrono::duration_cast<std::chrono::milliseconds>(listXmlStopwatch.elapsedDuration());

		// report -listxml status
		std::cout << "MAME -listxml invocation complete (XML size " << listXmlBytes.size() << " bytes; elapsed duration " << listXmlDuration.count() << "ms)" << std::endl;
		QVERIFY(listXmlBytes.size() > 0);

		// and build a QBuffer and assign it to listXmlSource
		listXmlSource = std::make_unique<QBuffer>(&listXmlBytes);
		QVERIFY(listXmlSource->open(QIODevice::ReadOnly));
	}
	else
	{
		// simultaneous -listxml and info DB build
		std::cout << "Running -listxml and info DB build simultaneously..." << std::endl;
		std::unique_ptr<QProcess> process = std::make_unique<QProcess>();
		invokeListXml(*process, program);
		listXmlSource = std::move(process);
	}

	// report that MAME has started
	std::cout << "Processing started...\r";

    // build info DB
    QByteArray infoDbBytes;
    Stopwatch buildInfoDbStopwatch;
    {
        // and process the output
        QString errorMessage;
        info::database_builder builder;
        QVERIFY(builder.process_xml(*listXmlSource, errorMessage, processXmlCallback));
        QVERIFY(errorMessage.isEmpty());

        // and put it in the byte array
        QBuffer buffer(&infoDbBytes);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        builder.emit_info(buffer);
    }   
    auto buildInfoDbDuration = std::chrono::duration_cast<std::chrono::milliseconds>(buildInfoDbStopwatch.elapsedDuration());
    listXmlSource.reset();

    // report our status and sanity checks
    std::cout << "Info DB build complete (database size " << infoDbBytes.size() << " bytes; elapsed duration " << buildInfoDbDuration.count() << "ms)" << std::endl;
    QVERIFY(infoDbBytes.size() > 0);

    // prepare buffer for reading
    QBuffer buffer(&infoDbBytes);
    QVERIFY(buffer.open(QIODevice::ReadOnly));

    // and process it, validating we've done so successfully
    info::database db;
    Stopwatch readInfoDbStopwatch;
    {
        bool dbChanged = false;
        db.addOnChangedHandler([&dbChanged]() { dbChanged = true; });
        QVERIFY(db.load(buffer));
        QVERIFY(dbChanged);
    }
    auto readInfoDbDuration = std::chrono::duration_cast<std::chrono::milliseconds>(readInfoDbStopwatch.elapsedDuration());

    // report our status and sanity checks
    std::cout << "Info DB read complete (" << db.machines().size() << " total machines; elapsed duration " << readInfoDbDuration.count() << "ms)" << std::endl;
    QVERIFY(db.machines().size() > 0);
}


//-------------------------------------------------
//  runAndExcerciseListXml - test our ability to
//  invoke and handle -listxml output
//-------------------------------------------------

int runAndExcerciseListXml(int argc, char *argv[], bool separateTasks)
{
    // identify the program
    QString program = argv[0];

    int result;
    try
    {
        internalRunAndExcerciseListXml(program, separateTasks);
        result = 0;
    }
    catch (std::exception &ex)
    {
        std::cout << "EXCEPTION: " << ex.what() << std::endl;
        result = 1;
    }
    return result;
}
