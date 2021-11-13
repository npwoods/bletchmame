/***************************************************************************

    utility.cpp

    Miscellaneous utility code

***************************************************************************/

#include <sstream>
#include <QDir>
#include <QGridLayout>

#include "utility.h"


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

const QString util::g_empty_string;


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
		if (!hiDigit.has_value())
			break;
		std::optional<std::uint8_t> loDigit = hexDigit(hex[pos * 2 + 1]);
		if (!loDigit.has_value())
			break;

		// and store the value
		dest[pos++] = (hiDigit.value() << 4) | (loDigit.value() << 0);
	}
	return pos;
}


//-------------------------------------------------
//  wxFileName::IsPathSeparator
//-------------------------------------------------

bool wxFileName::IsPathSeparator(QChar ch)
{
	return ch == '/' || ch == QDir::separator();
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
