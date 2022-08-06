/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

// bletchmame headers
#include "utility.h"

// Qt headers
#include <QDir>
#include <QGridLayout>
#include <QRegularExpression>

// standard headers
#include <sstream>


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const QString util::g_empty_string;

#ifdef Q_OS_WINDOWS
static const QChar s_pathListSeparator = ';';
#else // !Q_OS_WINDOWS
static const QRegularExpression s_pathListSeparator("\\:|\\;");
#endif // Q_OS_WINDOWS


//-------------------------------------------------
//  hexDigit
//-------------------------------------------------

static std::optional<uint8_t> hexDigit(char8_t ch)
{
	std::optional<uint8_t> result;
	switch (ch)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		result = ch - '0';
		break;

	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		result = ch - 'A' + 10;
		break;

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		result = ch - 'a' + 10;
		break;

	default:
		result = { };
		break;
	}
	return result;
}


//-------------------------------------------------
//  bytesFromHex
//-------------------------------------------------

std::size_t util::bytesFromHex(std::span<uint8_t> &dest, std::u8string_view hex)
{
	std::size_t pos = 0;
	while (pos < dest.size() && pos < hex.size() / 2)
	{
		// parse two hex digits
		std::optional<std::uint8_t> hiDigit = hexDigit(hex[pos * 2 + 0]);
		if (!hiDigit)
			break;
		std::optional<std::uint8_t> loDigit = hexDigit(hex[pos * 2 + 1]);
		if (!loDigit)
			break;

		// and store the value
		dest[pos++] = (*hiDigit << 4) | (*loDigit << 0);
	}
	return pos;
}


//-------------------------------------------------
//  toQString
//-------------------------------------------------

QString util::toQString(std::u8string_view s)
{
	return QString::fromUtf8(s.data(), s.size());
}


//-------------------------------------------------
//  toU8String
//-------------------------------------------------

std::u8string util::toU8String(const QString &s)
{
	QByteArray byteArray = s.toUtf8();
	return std::u8string((const char8_t *)byteArray.data(), (size_t)byteArray.size());
}


//-------------------------------------------------
//  trim
//-------------------------------------------------

std::u8string_view util::trim(std::u8string_view s)
{
	auto front_iter = std::find_if(s.begin(), s.end(), [](char8_t ch) { return !isspace(ch); });
	auto back_iter = front_iter != s.end()
		? std::find_if(s.rbegin(), s.rend(), [](char8_t ch) { return !isspace(ch); }).base()
		: s.end();
	return std::u8string_view(front_iter, back_iter);
}


//-------------------------------------------------
//  globalPositionBelowWidget
//-------------------------------------------------

QPoint globalPositionBelowWidget(const QWidget &widget)
{
	QPoint localPos(0, widget.height());
	QPoint globalPos = widget.mapToGlobal(localPos);
	return globalPos;
}


//-------------------------------------------------
//  truncateGridLayout
//-------------------------------------------------

void truncateGridLayout(QGridLayout &gridLayout, int rows)
{
	for (int row = gridLayout.rowCount() - 1; row >= 0 && row >= rows; row--)
	{
		for (int col = 0; col < gridLayout.columnCount(); col++)
		{
			QLayoutItem *layoutItem = gridLayout.itemAtPosition(row, col);
			if (layoutItem && layoutItem->widget())
				delete layoutItem->widget();
		}
	}
}


//-------------------------------------------------
//  setPixmapDevicePixelRatioToFit
//-------------------------------------------------

void setPixmapDevicePixelRatioToFit(QPixmap &pixmap, QSize size)
{
	qreal xScaleFactor = pixmap.width() / (qreal)size.width();
	qreal yScaleFactor = pixmap.height() / (qreal)size.height();
	qreal scaleFactor = std::max(xScaleFactor, yScaleFactor);
	pixmap.setDevicePixelRatio(scaleFactor);
}


//-------------------------------------------------
//  setPixmapDevicePixelRatioToFit
//-------------------------------------------------

void setPixmapDevicePixelRatioToFit(QPixmap &pixmap, int dimension)
{
	setPixmapDevicePixelRatioToFit(pixmap, QSize(dimension, dimension));
}


//-------------------------------------------------
//  getUniqueFileName
//-------------------------------------------------

QFileInfo getUniqueFileName(const QString &directory, const QString &baseName, const QString &suffix)
{
	QFileInfo result;
	QDir dir(directory);
	int attempt = 0;

	do
	{
		// set up the filename
		QString fileNameTemplate = attempt > 1
			? "%1 (%3).%2"
			: "%1.%2";
		QString fileName = fileNameTemplate.arg(baseName, suffix, QString::number(attempt));

		// set up the QFileInfo
		result = QFileInfo(dir, fileName);

		// bump the attempt
		attempt++;
	} while (result.exists());

	return result;
}


//-------------------------------------------------
//  areFileInfosEquivalent
//-------------------------------------------------

bool areFileInfosEquivalent(const QFileInfo &fi1, const QFileInfo &fi2)
{
	// we're trying to see if these two file infos are equivalent; if the files exist we
	// can ask the operating system, but if either are not present we have to compare the
	// paths (fundamentally this is not a foolproof operation)
	return fi1.exists() && fi2.exists()
		? fi1 == fi2
		: fi1.absoluteFilePath() == fi2.absoluteFilePath();
}


//-------------------------------------------------
//  splitPathList
//-------------------------------------------------

QStringList splitPathList(const QString &s)
{
	return s.split(s_pathListSeparator, Qt::SkipEmptyParts);
}


//-------------------------------------------------
//  joinPathList
//-------------------------------------------------

QString joinPathList(const QStringList &list)
{
	// MAME uses ';' as its primary path list separator, even on non-Windows platforms
	return list.join(';');
}
