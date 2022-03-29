/***************************************************************************

    bindata.h

    Generic "binary data" database code

***************************************************************************/

#pragma once

#ifndef BINDATA_H
#define BINDATA_H

// bletchmame headers
#include "utility.h"

// Qt headers
#include <QString>

// standard headers
#include <compare>
#include <optional>
#include <span>


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
		view_position(std::size_t offset = 0, std::size_t count = 0)
			: m_offset(util::safe_static_cast<std::uint32_t>(offset))
			, m_count(util::safe_static_cast<std::uint32_t>(count))
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
		view() : m_db(nullptr)
		{
		}

		view(const TDatabase &db, const view_position &pos)
			: m_db(&db)
		{
			m_db->getDataSpan(m_span, pos.offset(), pos.count());
		}

		view(const view &) = default;
		view(view &&) = default;

		view &operator=(const view &that) = default;
		bool operator==(const view &) const = default;

		TPublic operator[](std::size_t position) const
		{
			return *(begin() + position);
		}

		size_t size() const { return m_span.size(); }
		bool empty() const { return m_span.empty(); }

		view subview(std::size_t offset, std::size_t count) const
		{
			return view(m_db, m_span.subspan(offset, count));
		}

		// ======================> view::iterator
		class iterator
		{
		public:
			iterator()
				: m_db(nullptr)
			{
			}
			iterator(const TDatabase *db, typename std::span<const TBinary>::iterator &&spanIter)
				: m_db(db)
				, m_spanIter(std::move(spanIter))
			{
			}
			iterator(const iterator &) = default;
			iterator(iterator &&) = default;
			iterator &operator=(const iterator &) = default;

			using iterator_category = std::random_access_iterator_tag;
			using value_type = TPublic;
			using difference_type = ptrdiff_t;
			using pointer = TPublic *;
			using reference = TPublic &;

			TPublic operator*() const
			{
				return TPublic(*m_db, *m_spanIter);
			}
				
			std::optional<TPublic> operator->() const { return **this; }

			TPublic operator[](difference_type offs) const
			{
				return *(*this + offs);
			}

			auto operator<=>(const iterator &that) const
			{
				asset_compatible_iterator(that);
				return m_spanIter <=> that.m_spanIter;
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

			iterator &operator++()
			{
				m_spanIter++;
				return *this;
			}

			iterator operator++(int)
			{
				auto iter = *this;
				m_spanIter++;
				return iter;
			}

			iterator &operator--()
			{
				m_spanIter--;
				return *this;
			}

			iterator operator--(int)
			{
				auto iter = *this;
				m_spanIter--;
				return iter;
			}

			iterator &operator+=(difference_type offs)
			{
				m_spanIter += offs;
				return *this;
			}

			iterator &operator-=(difference_type offs)
			{
				m_spanIter -= offs;
				return *this;
			}

			iterator operator+(difference_type offs) const
			{
				auto iter = *this;
				iter.m_spanIter += offs;
				return iter;
			}

			iterator operator-(difference_type offs) const
			{
				auto iter = *this;
				iter.m_spanIter -= offs;
				return iter;
			}

			difference_type operator-(const iterator &that) const
			{
				asset_compatible_iterator(that);
				return m_spanIter - that.m_spanIter;
			}

			friend iterator operator+(difference_type lhs, const iterator &rhs) { return rhs + lhs; }
			friend iterator operator-(difference_type lhs, const iterator &rhs) { return rhs + lhs; }

		private:
			const TDatabase *							m_db;
			typename std::span<const TBinary>::iterator	m_spanIter;

			void asset_compatible_iterator(const iterator &that) const
			{
				assert(m_db == that.m_db);
				(void)that;
			}
		};

		typedef iterator const_iterator;

		iterator begin() const			{ return iterator(m_db, m_span.begin()); }
		iterator end() const			{ return iterator(m_db, m_span.end()); }
		const_iterator cbegin() const	{ return begin(); }
		const_iterator cend() const		{ return end(); }

	private:
		const TDatabase *			m_db;
		std::span<const TBinary>	m_span;

		view(const TDatabase *db, typename std::span<const TBinary> &&span)
			: m_db(db)
			, m_span(std::move(span))
		{
		}
	};
}

template <typename TDatabase, typename TPublic, typename TBinary>
inline constexpr bool std::ranges::enable_borrowed_range<bindata::view<TDatabase, TPublic, TBinary>> = true;

#endif // BINDATA_H
