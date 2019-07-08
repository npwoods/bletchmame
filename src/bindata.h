/***************************************************************************

    bindata.h

    Generic "binary data" database code

***************************************************************************/

#pragma once

#ifndef BINDATA_H
#define BINDATA_H

#include <wx/string.h>


//**************************************************************************
//  TYPE DECLARATIONS
//**************************************************************************

namespace bindata
{
	template<typename TDatabase, typename TPublic, typename TBinary>
	class view;

	// ======================> entry
	template<typename TDatabase, typename TPublic, typename TBinary>
	class entry
	{
	public:
		using view = view<TDatabase, TPublic, TBinary>;

	protected:
		entry(const TDatabase &db, const TBinary &inner)
			: m_db(db)
			, m_inner(inner)
		{
		}

		const TDatabase &db() const { return m_db; }
		const TBinary &inner() const { return m_inner; }
		const wxString &get_string(std::uint32_t strindex) const { return db().get_string(strindex); }

	private:
		const TDatabase &	m_db;
		const TBinary &		m_inner;
	};

	// ======================> view
	template<typename TDatabase, typename TPublic, typename TBinary>
	class view
	{
	public:
		view() : m_db(nullptr), m_offset(0), m_count(0) { }
		view(const TDatabase &db, size_t offset, std::uint32_t count) : m_db(&db), m_offset(offset), m_count(count) { }
		view(const view &that) = default;
		view(view &&that) = default;

		view &operator=(const view &that)
		{
			m_db = that.m_db;
			m_offset = that.m_offset;
			m_count = that.m_count;
			return *this;
		}

		bool operator==(const view &that) const
		{
			return m_db == that.m_db
				&& m_offset == that.m_offset
				&& m_count == that.m_count;
		}

		TPublic operator[](std::uint32_t position) const
		{
			if (position >= m_count)
				throw false;
			const std::uint8_t *ptr = &m_db->m_data.data()[m_offset + position * sizeof(TBinary)];
			return TPublic(*m_db, *reinterpret_cast<const TBinary *>(ptr));
		}

		size_t size() const { return m_count; }

		view subview(std::uint32_t index, std::uint32_t count) const
		{
			if (index >= m_count || (index + count > m_count))
				throw false;
			return count > 0
				? view(*m_db, m_offset + index * sizeof(TBinary), count)
				: view();
		}

		// ======================> view::iterator
		class iterator
		{
		public:
			iterator(const view &view, uint32_t position)
				: m_view(view)
				, m_position(position)
			{
			}

			using iterator_category = std::random_access_iterator_tag;
			using value_type = TPublic;
			using difference_type = ssize_t;
			using pointer = TPublic * ;
			using reference = TPublic & ;

			TPublic operator*() const { return m_view[m_position]; }
			TPublic operator->() const { return m_view[m_position]; }

			bool operator<(const iterator &that)
			{
				asset_compatible_iterator(that);
				return m_position < that.m_position;
			}

			bool operator!=(const iterator &that)
			{
				asset_compatible_iterator(that);
				return m_position != that.m_position;
			}

			iterator &operator=(const iterator &that)
			{
				asset_compatible_iterator(that);
				m_position = that.m_position;
				return *this;
			}

			void operator++()
			{
				m_position++;
			}

			ssize_t operator-(const iterator &that)
			{
				asset_compatible_iterator(that);
				return ((ssize_t)m_position) - ((ssize_t)that.m_position);
			}

		private:
			view				m_view;
			uint32_t			m_position;

			void asset_compatible_iterator(const iterator &that)
			{
				assert(m_view == that.m_view);
				(void)that;
			}
		};

		typedef iterator const_iterator;

		iterator begin() const { return iterator(*this, 0); }
		iterator end() const { return iterator(*this, m_count); }
		const_iterator cbegin() const { return begin(); }
		const_iterator cend() const { return end(); }

	private:
		const TDatabase *	m_db;
		size_t				m_offset;
		uint32_t			m_count;
	};
};

#endif // BINDATA_H
