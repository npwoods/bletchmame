/***************************************************************************

	7zip.cpp

	Logic for handling 7-Zip (*.7z)

***************************************************************************/

// bletchmame headers
#include "7zip.h"
#include "perfprofiler.h"

// Qt headers
#include <QBuffer>
#include <QFile>

// liblzma headers
#include <lzma/7z.h>
#include <lzma/7zAlloc.h>
#include <lzma/7zFile.h>
#include <lzma/7zCrc.h>
#include <lzma/7zTypes.h>


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

namespace
{
	class CrcTableGenerator
	{
	public:
		CrcTableGenerator()
		{
			CrcGenerateTable();
		}
	};
}

class SevenZipFile::Impl : private ISeekInStream
{
public:
	typedef std::unique_ptr<Impl> ptr;

	// ctor/dtor
	Impl();
	Impl(const Impl &) = delete;
	Impl(Impl &&) = delete;
	~Impl();

	// methods
	bool open(std::unique_ptr<QIODevice> &&stream);
	void close();
	std::unique_ptr<QIODevice> extract(int index);
	int entryCount() const;
	QString entryName(int index) const;
	bool entryIsDirectory(int index) const;
	std::optional<std::uint32_t> entryCrc32(int index) const;

private:
	static const ISzAlloc			s_allocImpl;
	static const ISzAlloc			s_allocTempImpl;
	static const CrcTableGenerator	s_crcTableGenerator;

	std::unique_ptr<QIODevice>		m_stream;
	UInt32							m_blockIndex;
	Byte *							m_outBuffer;
	std::size_t						m_outBufferSize;
	CSzArEx							m_db;
	CLookToRead2					m_lookToRead;
	Byte							m_lookToReadBuffer[4096];

	// ILookInStream implementation
	SRes doRead(void *buf, size_t *size);
	SRes doSeek(Int64 *pos, ESzSeek origin);

	// trampolines
	static Impl &getImpl(const ISeekInStream *seekInStream);
	static SRes readTrampoline(const ISeekInStream *p, void *buf, size_t *size);
	static SRes seekTrampoline(const ISeekInStream *p, Int64 *pos, ESzSeek origin);
};


//**************************************************************************
//  STATIC VARIABLES
//**************************************************************************

const ISzAlloc SevenZipFile::Impl::s_allocImpl = { SzAlloc, SzFree };
const ISzAlloc SevenZipFile::Impl::s_allocTempImpl = { SzAllocTemp, SzFreeTemp };
const CrcTableGenerator SevenZipFile::Impl::s_crcTableGenerator;


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

//-------------------------------------------------
//  ctor
//-------------------------------------------------

SevenZipFile::SevenZipFile()
	: m_cursor(0)
{
}


//-------------------------------------------------
//  dtor
//-------------------------------------------------

SevenZipFile::~SevenZipFile()
{
}


//-------------------------------------------------
//  open(const QString &path)
//-------------------------------------------------

bool SevenZipFile::open(const QString &path)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// create the file
	std::unique_ptr<QFile> file = std::make_unique<QFile>(path);
	if (!file->open(QFile::ReadOnly))
		return false;

	// create the impl and initialize it
	m_impl = std::make_unique<Impl>();
	if (!m_impl->open(std::move(file)))
		return false;

	// prepare our file indexes
	int entryCount = m_impl->entryCount();
	m_filesByName.clear();
	m_filesByCrc32.clear();
	m_filesByName.reserve(entryCount);
	m_filesByCrc32.reserve(entryCount);
	m_cursor = 0;

	// and we're done
	return true;
}


//-------------------------------------------------
//  get(const QString &)
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::get(const QString &fileName)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// find this file
	QString normalizedFileName = normalizeFileName(fileName);
	auto iter = m_filesByName.find(normalizedFileName);

	// extract or populate as appropriate
	std::optional<int> index = iter != m_filesByName.end()
		? iter->second
		: std::optional<int>();
	return extractOrPopulate(index, normalizedFileName, { });
}


//-------------------------------------------------
//  get(std::uint32_t crc32)
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::get(std::uint32_t crc32)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// find this file
	auto iter = m_filesByCrc32.find(crc32);

	// extract or populate as appropriate
	std::optional<int> index = iter != m_filesByCrc32.end()
		? iter->second
		: std::optional<int>();
	return extractOrPopulate(index, { }, crc32);
}


//-------------------------------------------------
//  normalizeFileName
//-------------------------------------------------

QString SevenZipFile::normalizeFileName(const QString &fileName)
{
	return fileName.toUpper();
}


//-------------------------------------------------
//  extractOrPopulate
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::extractOrPopulate(
	std::optional<int> index,
	const std::optional<QString> &targetNormalizedFileName,
	std::optional<std::uint32_t> targetCrc32)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// when we're first called, we _might_ already have the result; we might equally
	// find the result in the process of iterating
	while(!index && m_cursor < m_impl->entryCount())
	{
		// only look at this entry if its a directory
		if (!m_impl->entryIsDirectory(m_cursor))
		{
			// add this filename to the map
			QString thisNormalizedFileName = normalizeFileName(m_impl->entryName(m_cursor));
			m_filesByName.emplace(thisNormalizedFileName, m_cursor);

			// and do the same for the CRC-32
			std::optional<std::uint32_t> thisCrc32 = m_impl->entryCrc32(m_cursor);
			if (thisCrc32)
				m_filesByCrc32.emplace(*thisCrc32, m_cursor);

			// is this the target?
			if ((!targetNormalizedFileName || targetNormalizedFileName == thisNormalizedFileName)
				&& (!targetCrc32 || targetCrc32 == thisCrc32))
			{
				index = m_cursor;
			}
		}

		// next item
		m_cursor++;
	}

	// we're done; do we have an index to extract?
	return index
		? m_impl->extract(*index)
		: std::unique_ptr<QIODevice>();
}


//-------------------------------------------------
//  Impl ctor
//-------------------------------------------------

SevenZipFile::Impl::Impl()
	: m_outBuffer(nullptr)
{
	// initialize archive
	SzArEx_Init(&m_db);

	// initialize CLookToRead2
	memset(&m_lookToRead, 0, sizeof(m_lookToRead));
	LookToRead2_CreateVTable(&m_lookToRead, false);
	LookToRead2_Init(&m_lookToRead);
	m_lookToRead.realStream = this;
	m_lookToRead.buf = m_lookToReadBuffer;
	m_lookToRead.bufSize = sizeof(m_lookToReadBuffer);

	// initalize ISeekInStream with trampolines
	Read = readTrampoline;
	Seek = seekTrampoline;
}


//-------------------------------------------------
//  Impl dtor
//-------------------------------------------------

SevenZipFile::Impl::~Impl()
{
	close();
	SzArEx_Free(&m_db, &s_allocImpl);
}


//-------------------------------------------------
//  Impl::open
//-------------------------------------------------

bool SevenZipFile::Impl::open(std::unique_ptr<QIODevice> &&stream)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	close();
	m_stream = std::move(stream);
	m_blockIndex = 0;
	m_outBufferSize = 0;
	SRes res = SzArEx_Open(&m_db, &m_lookToRead.vt, &s_allocImpl, &s_allocTempImpl);
	return res == SZ_OK;
}


//-------------------------------------------------
//  Impl::close
//-------------------------------------------------

void SevenZipFile::Impl::close()
{
	m_stream.reset();

	if (m_outBuffer)
	{
		IAlloc_Free(&s_allocImpl, m_outBuffer);
		m_outBuffer = nullptr;
	}
}


//-------------------------------------------------
//  Impl::extract
//-------------------------------------------------

std::unique_ptr<QIODevice> SevenZipFile::Impl::extract(int index)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// perform the extraction
	std::size_t offset = 0;
	std::size_t sizeProcessed = 0;
	SRes res = SzArEx_Extract(&m_db, &m_lookToRead.vt, index, &m_blockIndex, &m_outBuffer, &m_outBufferSize, &offset, &sizeProcessed, &s_allocImpl, &s_allocTempImpl);
	if (res != SZ_OK)
		return { };

	// create a buffer to return
	std::unique_ptr<QBuffer> result = std::make_unique<QBuffer>();
	QByteArray &byteArray = result->buffer();

	// resize the returned buffer appropriately
	int intSizeProcessed = (int)sizeProcessed;
	if (sizeProcessed != intSizeProcessed)
		return { };
	byteArray.resize(intSizeProcessed);

	// copy the buffer and return
	memcpy(byteArray.data(), m_outBuffer + offset, sizeProcessed);
	return result;
}


//-------------------------------------------------
//  Impl::entryCount
//-------------------------------------------------

int SevenZipFile::Impl::entryCount() const
{
	return m_db.NumFiles;
}


//-------------------------------------------------
//  Impl::entryName
//-------------------------------------------------

QString SevenZipFile::Impl::entryName(int index) const
{
	std::size_t nameLen = SzArEx_GetFileNameUtf16(&m_db, index, nullptr);

	QString name;
	name.resize(nameLen);
	SzArEx_GetFileNameUtf16(&m_db, index, (UInt16 *)&name[0]);

	// get rid of trailing NUL, if present
	if (nameLen > 0 && name[nameLen - 1] == QChar('\0'))
		name.resize(nameLen - 1);

	return name;
}


//-------------------------------------------------
//  Impl::entryIsDirectory
//-------------------------------------------------

bool SevenZipFile::Impl::entryIsDirectory(int index) const
{
	return SzArEx_IsDir(&m_db, index);
}


//-------------------------------------------------
//  Impl::entryCrc32
//-------------------------------------------------

std::optional<std::uint32_t> SevenZipFile::Impl::entryCrc32(int index) const
{
	std::optional<std::uint32_t> result;
	if (SzBitArray_Check(m_db.CRCs.Defs, index))
		result = m_db.CRCs.Vals[index];
	return result;
}


//-------------------------------------------------
//  Impl::doRead
//-------------------------------------------------

SRes SevenZipFile::Impl::doRead(void *buf, size_t *size)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	qint64 byteCount = *size;
	*size = m_stream->read(reinterpret_cast<char *>(buf), byteCount);
	return *size == byteCount ? SZ_OK : SZ_ERROR_READ;
}


//-------------------------------------------------
//  Impl::doSeek
//-------------------------------------------------

SRes SevenZipFile::Impl::doSeek(Int64 *pos, ESzSeek origin)
{
	ProfilerScope prof(CURRENT_FUNCTION);

	// determine the absolute position
	qint64 qpos;
	switch (origin)
	{
	case SZ_SEEK_SET:
		qpos = *pos;
		break;
	case SZ_SEEK_CUR:
		qpos = *pos + m_stream->pos();
		break;
	case SZ_SEEK_END:
		qpos = *pos + m_stream->size();
		break;
	default:
		throw false;
	}

	// seek!
	if (!m_stream->seek(qpos))
		return SZ_ERROR_UNSUPPORTED;

	// and we've succeeded
	*pos = m_stream->pos();
	return SZ_OK;
}


//**************************************************************************
//  TRAMPOLINES
//**************************************************************************

//-------------------------------------------------
//  Impl::getImpl
//-------------------------------------------------

SevenZipFile::Impl &SevenZipFile::Impl::getImpl(const ISeekInStream *seekInStream)
{
	ISeekInStream *nonConstSeekInStream = const_cast<ISeekInStream *>(seekInStream);
	return *reinterpret_cast<Impl *>(nonConstSeekInStream);
}


//-------------------------------------------------
//  Impl::readTrampoline
//-------------------------------------------------

SRes SevenZipFile::Impl::readTrampoline(const ISeekInStream *p, void *buf, size_t *size)
{
	return getImpl(p).doRead(buf, size);
}


//-------------------------------------------------
//  Impl::seekTrampoline
//-------------------------------------------------

SRes SevenZipFile::Impl::seekTrampoline(const ISeekInStream *p, Int64 *pos, ESzSeek origin)
{
	return getImpl(p).doSeek(pos, origin);
}
