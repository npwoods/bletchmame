/***************************************************************************

    bindata.h

    Generic "binary data" database code

***************************************************************************/

#pragma once

#ifndef BINDATA_H
#define BINDATA_H

#include <QString>
#include <optional>


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
		using view = bindata::view<TDatabase, TPublic, TBinary>;

	protected:
		entry(const TDatabase &db, const TBinary &inner)
			: m_db(&db)
			, m_inner(&inner)
		{
		}

		const TDatabase &db() const { return *m_db; }
		const TBinary &inner() const { return *m_inner; }
		const QString &get_string(std::uint32_t strindex) const { return db().get_string(strindex); }

	private:
		const TDatabase *	m_db;
		const TBinary *		m_inner;
	};


	// ======================> view_position
	class view_position
	{
	public:
		view_position(std::uint32_t offset = 0, std::uint32_t count = 0)
			: m_offset(offset)
			, m_count(count)
		{
		}

		void clear()
		{
			m_offset = 0;
			m_count = 0;
		}

		std::uint32_t offset() const { return m_offset; }
		std::uint32_t count() const { return m_count; }

	private:
		std::uint32_t	m_offset;
		std::uint32_t	m_count;
	};


	// ======================> view
	template<typename TDatabase, typename TPublic, typename TBinary>
	class view
	{
	public:
		view() : m_db(nullptr), m_offset(0), m_count(0) { }
		view(const TDatabase &db, const view_position &pos) : m_db(&db), m_offset(pos.offset()), m_count(pos.count()) { }
		view(const view &that) = default;
		view(view &&that) = default;

		view &operator=(const view &that)
		{
			m_db = that.m_db;
			m_offset = that.m_offset;
			m_count = that.m_count;
			return *this;
		}

		bool operator==(const view &that) const = default;

		TPublic operator[](std::uint32_t position) const
		{
			if (position >= m_count)
				throw false;
			const std::uint8_t *ptr = &m_db->m_state.m_data.data()[m_offset + position * sizeof(TBinary)];
			return TPublic(*m_db, *reinterpret_cast<const TBinary *>(ptr));
		}

		size_t size() const { return m_count; }
		bool empty() const { return size() == 0; }

		view subview(std::uint32_t index, std::uint32_t count) const
		{
			if (index > m_count || (index + count > m_count))
				throw false;
			return count > 0
				? view(*m_db, view_position(m_offset + index * sizeof(TBinary), count))
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
			using difference_type = ptrdiff_t;
			using pointer = TPublic * ;
			using reference = TPublic & ;

			TPublic operator*() const { return m_view[m_position]; }
			std::optional<TPublic> operator->() const { return m_view[m_position]; }

			auto operator<=>(const iterator &that) const
			{
				asset_compatible_iterator(that);
				return m_position <=> that.m_position;
			}

			bool operator==(const iterator &that) const
			{
				// not sure why the compiler can't deduce this
				return (*this <=> that) == 0;
			}

			bool operator!=(const iterator &that) const
			{
				// not sure why the compiler can't deduce this
				return (*this <=> that) != 0;
			}

			iterator &operator=(const iterator &that)
			{
				asset_compatible_iterator(that);
				m_position = that.m_position;
				return *this;
			}

			iterator &operator++()
			{
				m_position++;
				return *this;
			}

			iterator operator++(int)
			{
				auto iter = *this;
				m_position++;
				return iter;
			}

			iterator &operator--()
			{
				m_position--;
				return *this;
			}

			iterator operator--(int)
			{
				auto iter = *this;
				m_position--;
				return iter;
			}

			iterator &operator+=(uint32_t offs)
			{
				m_position += offs;
				return *this;
			}

			ptrdiff_t operator-(const iterator &that)
			{
				asset_compatible_iterator(that);
				return ((ptrdiff_t)m_position) - ((ptrdiff_t)that.m_position);
			}

		private:
			view				m_view;
			uint32_t			m_position;

			void asset_compatible_iterator(const iterator &that) const
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
