/***************************************************************************

    mamerunner.cpp

    Testing infrastructure

***************************************************************************/

// bletchmame headers
#include "test.h"
#include "mameversion.h"
#include "mameworkercontroller.h"
#include "xmlparser.h"

// Qt headers
#include <QDir>
#include <QProcess>
#include <QThread>

// standard headers
#include <iostream>
#include <stdexcept>


//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define HAS_COLOR   1


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  getAnsiColorCodeForChatterType
//-------------------------------------------------

static const char *getAnsiColorCodeForChatterType(MameWorkerController::ChatterType chatterType)
{
    const char *result;
    switch (chatterType)
    {
    case MameWorkerController::ChatterType::Command:
        result = "\x1B[36m";
        break;
    case MameWorkerController::ChatterType::GoodResponse:
        result = "\x1B[92m";
        break;
    case MameWorkerController::ChatterType::ErrorResponse:
        result = "\x1B[91m";
        break;
    default:
        throw false;
    }
    return result;
}


//-------------------------------------------------
//  chatter
//-------------------------------------------------

static void chatter(MameWorkerController::ChatterType chatterType, const QString &text)
{
    if (HAS_COLOR)
        std::cout << getAnsiColorCodeForChatterType(chatterType);
    std::cout << text.trimmed().toLocal8Bit().constData() << std::endl;
}


//-------------------------------------------------
//  checkResponseForParseError
//-------------------------------------------------

static void checkResponseForParseError(const MameWorkerController::Response &response)
{
    if (response.m_update.has_value() && !response.m_update->m_parse_error.isEmpty())
        throw std::logic_error(QString("Error parsing status update: %1").arg(response.m_update->m_parse_error).toLocal8Bit());
}


//-------------------------------------------------
//  issueCommandAndReceiveResponse
//-------------------------------------------------

static MameWorkerController::Response issueCommandAndReceiveResponse(MameWorkerController &controller, std::u8string_view command)
{
    // issue the command
    controller.issueCommand(util::toQString(command));

    // and get a response
    MameWorkerController::Response response = controller.receiveResponse();
    if (response.m_type != MameWorkerController::Response::Type::Ok)
        throw std::logic_error(QString("Received invalid response from MAME: %1").arg(response.m_text).toLocal8Bit());
    checkResponseForParseError(response);
    return response;
}


//-------------------------------------------------
//  getScriptRequiredMameVersion
//-------------------------------------------------

static std::optional<SimpleMameVersion> getScriptRequiredMameVersion(const QString &scriptFileName)
{
    std::optional<SimpleMameVersion> result;
    XmlParser xml;
    xml.onElementBegin({ "script" }, [&result](const XmlParser::Attributes &attributes)
    {
        std::optional<QString> requiredVersionString = attributes.get<QString>("requiredVersion");
		if (requiredVersionString)
		{
			MameVersion requiredVersion(*requiredVersionString);
			result = SimpleMameVersion(requiredVersion.major(), requiredVersion.minor());
		}
    });

    if (!xml.parse(scriptFileName))
        throw std::logic_error(QString("Could not load script '%1'").arg(scriptFileName).toLocal8Bit().constData());
    return result;
}


//-------------------------------------------------
//  getMameVersion
//-------------------------------------------------

static MameVersion getMameVersion(const QString &program)
{
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start(program, QStringList() << "-version");
    process.waitForFinished();
    QByteArray output = process.readAllStandardOutput();
    QString versionString = QString::fromUtf8(output);
    return MameVersion(versionString);
}


//-------------------------------------------------
//  trim
//-------------------------------------------------

static void trim(std::u8string &s)
{
    auto begin = std::find_if(s.begin(), s.end(), [](char c) { return !isspace(c); });
    auto end = std::find_if(s.rbegin(), s.rend(), [](char c) { return !isspace(c); });
    if (begin != s.begin() || end != s.rbegin())
    {
        s = (begin < end.base())
            ? std::u8string(begin, end.base())
            : std::u8string();
    }
}


//-------------------------------------------------
//  internalRunAndExcerciseMame
//-------------------------------------------------

static void internalRunAndExcerciseMame(const QString &scriptFileName, const QString &program, const QStringList &arguments)
{
    // check to see if the program file exists
    QFileInfo fileInfo(program);
    if (!fileInfo.isFile())
        throw std::logic_error(QString("MAME program '%1' not found").arg(program).toLocal8Bit().constData());

    // identify the script we're running
    std::cout << QString("Running script: %1").arg(scriptFileName).toStdString() << std::endl;

    // version check
    std::optional<SimpleMameVersion> requiredMameVersion = getScriptRequiredMameVersion(scriptFileName);
    if (requiredMameVersion)
    {
        MameVersion mameVersion = getMameVersion(program);
        if (!mameVersion.isAtLeast(*requiredMameVersion))
        {
            std::cout << QString("Script requires %1 (this is %2)").arg(requiredMameVersion->toPrettyString(), mameVersion.toPrettyString()).toStdString() << std::endl;
            return;
        }
    }

    // start the process
    QProcess process;
    process.setReadChannel(QProcess::StandardOutput);
    process.start(program, arguments);

    // report the process starting
    std::cout << QString("Started MAME: %1 %2").arg(program, arguments.join(" ")).toStdString() << std::endl;

    // start controlling MAME
    MameWorkerController controller(process, chatter);

    // read the initial response
    MameWorkerController::Response response = controller.receiveResponse();
    if (response.m_type != MameWorkerController::Response::Type::Ok)
    {
        QString errorMessage = !response.m_text.isEmpty()
            ? std::move(response.m_text)
            : controller.scrapeMameStartupError();
        throw std::logic_error(errorMessage.toLocal8Bit());
    }
    checkResponseForParseError(response);

    // get ready to parse
    XmlParser xml;
    xml.onElementEnd({ "script", "command" }, [&controller](std::u8string &&text)
    {
        // normalize the text
        trim(text);
        text += u8"\n";

        // issue the command
        issueCommandAndReceiveResponse(controller, text);
    });

    // load the script
    if (!xml.parse(scriptFileName))
        throw std::logic_error(QString("Could not load script '%1'").arg(scriptFileName).toLocal8Bit().constData());

    // wait for exit
    if (!process.waitForFinished())
        throw std::logic_error("waitForFinished() returned false");
}


//-------------------------------------------------
//  runAndExcerciseMame
//-------------------------------------------------

int runAndExcerciseMame(int argc, char *argv[])
{
    int currentArg = 0;

    // identify the scripts
    QFileInfoList scripts;
    QString scriptOrDirectory = argv[currentArg++];
    QFileInfo fi(scriptOrDirectory);
    if (fi.isFile())
    {
        scripts << std::move(fi);
    }
    else if (fi.isDir())
    {
        QDir scriptDir(scriptOrDirectory);
        scripts = scriptDir.entryInfoList(QStringList() << "*.xml", QDir::Files);
    }
    if (scripts.isEmpty())
        throw std::logic_error("No scripts");

    // identify the program
    QString program = argv[currentArg++];

    // identify the arguments
    QStringList arguments;
    while(currentArg < argc)
        arguments << argv[currentArg++];

    int result;
    try
    {
        for (const QFileInfo &script : scripts)
        {
            internalRunAndExcerciseMame(script.absoluteFilePath(), program, arguments);
        }
        result = 0;
    }
    catch (std::exception &ex)
    {
        std::cout << "EXCEPTION: " << ex.what() << std::endl;
        result = 1;
    }
    return result;
}
