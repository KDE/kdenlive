/***************************************************************************
                          rangelist.h  -  description
                             -------------------
    begin                : Sun Dec 1 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef RANGELIST_H
#define RANGELIST_H

#include <qvaluelist.h>

template<class T> class RangeList;


template<class T> class RangeListIterator {
public:
	RangeListIterator(RangeList<T> &range) :
		m_range(range.rangeList())
	{
		m_itt = m_range.begin();
	}

	~RangeListIterator() {
	}

	RangeListIterator<T> &operator=(const RangeListIterator<T> itt) {
		m_itt = itt.curItt();
	}

	RangeListIterator<T> &operator++() {
		++m_itt;
		++m_itt;
	}

	/** Returns true if we are at the end of the range list */
	bool finished() {
		return m_itt == m_range.end();
	}

	/** Returns the start of this range segment */
	T start() {
		return (*m_itt);
	}

	/** Returns the end of this range segment. */
	T end() {
		++m_itt;
		return (*(m_itt--));
	}

	QValueListIterator<T> &curItt() const {
		return m_itt;
	}
private:
	QValueList<T> &m_range;
	QValueListIterator<T> m_itt;
};

/**Holds a list of type, and holds ranges of values. These ranges never overlap - if a new range
overlaps with a previous range, the two become merged into one. When reading the list back,
we will never see part of a range expressed twice.

This class is useful for handling "dirty painting", as long as you are only working on a single
axis.
  *@author Jason Wood
  */

template<class T> class RangeList {
public:	
	RangeList() {
	}
	
	~RangeList() {
	}

	/** Clears the currently set ranges */
	void clear() {
		m_range.clear();
	}

	/** Sets the full range of values that we are going to consider - if any value ranges are added, they
	will be truncated so that they fit into this range. If the current range already falls outside of this
	range, then it will be truncated until it fits. */
	void setFullRange(T min, T max) {
		m_min = min;
		m_max = max;
#warning RangeList::setFullRange does not check that internal range conforms with new boundaries yet.
	}

	/** Adds a range of values to the list. This range will be merged with any range of values that
	 already exists. */
	void addRange(T start, T end) {
		
		if(start == end) return;
		if(start > end) {
			T temp = start;
			start = end;
			end = temp;
		}

		if(start >= m_max) return;
		if(end <= m_min) return;

		if(start < m_min) start = m_min;
		if(end > m_max) end = m_max;

		T cs, ce;		
		T ns, ne;
		int count;

		if(m_range.isEmpty()) {
			m_range.push_back(start);
			m_range.push_back(end);
			return;
		}

		// search for the correct place in the list, or a pair that overlaps.
		for(count = 0; count < m_range.count(); count+=2) {
			cs = m_range[count];
			ce = m_range[count+1];

			if(end <= cs) {
				m_range.insert(m_range.at(count), end);
				m_range.insert(m_range.at(count), start);
				break;
			}
			if((start <= ce) && (end >= cs)) {
				m_range[count] = (start < cs) ? start : cs;
				m_range[count+1] = (end > ce) ? end : ce;
				break;
			}
		}

		if(count == m_range.count()) {
			m_range.push_back(start);
			m_range.push_back(end);			
		} else {
			// Check the remaining parts don't overlap.
			while(count + 2 < m_range.count()) {
				cs = m_range[count];
				ce = m_range[count+1];
				ns = m_range[count+2];
				ne = m_range[count+3];

				if((cs <= ne) && (ns <= ce)) {
					m_range[count] = (cs < ns) ? cs : ns;
					m_range[count+1] = (ce > ne) ? ce : ne;
					m_range.remove(m_range.at(count+3));
					m_range.remove(m_range.at(count+2));
				} else {
					break;
				}
			}
		}
	}

	/** This should _not_ be public. */
	QValueList<T> &rangeList() {
		return m_range;
	}

	RangeListIterator<T> &begin() {
		return m_range.begin();
	}
private:
	QValueList<T> m_range;
	T m_min;
	T m_max;
};

#endif
