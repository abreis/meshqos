/* 
 *  Copyright (C) 2007 Dip. Ing. dell'Informazione, University of Pisa, Italy
 *  http://info.iet.unipi.it/~cng/ns2mesh80216/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA, USA
 */

#ifndef __NS2_WIMAX_CIRCULAR_H
#define __NS2_WIMAX_CIRCULAR_H

#include <list>
#include <assert.h>

//! Circular list of elements.
template<typename T>
class CircularList {
	// Friend of WimaxDebug so that it can print out lists.
	friend class WimaxDebug;

	//! Internal list structure.
	std::list<T> list_;

	//! Pointer to the current element.
	typename std::list<T >::iterator cur_;

	//! Number of elements.
	unsigned int size_;

public:
	//! Create an empty circular list.
	CircularList () { size_ = 0; }
	//! Do nothing.
	~CircularList () { }

	//! Return true if the list is empty.
	bool empty () { return ( size_ == 0 ); }

	//! Return the number of elements.
	unsigned int size () { return size_; }

	//! Return true if a given element is in the list.
	bool find (const T& t) {
		if ( size_ == 0 ) return false;
		typename std::list<T>::iterator it;
		for ( it = list_.begin() ; it != list_.end() && *it != t ; ++it ) { }
		if ( it == list_.end() ) return false;
		return true;
	}

	//! Finds an element in the list and return it.
	/*!
	  The element returned is only meaningful if valid == true.
	  */
	T& find (T& t, bool& valid) {
		if ( size_ == 0 ) { valid = false; return t; }
		typename std::list<T>::iterator it;
		for ( it = list_.begin() ; it != list_.end() && *it != t ; ++it ) { }
		if ( it == list_.end() ) { valid = false; return t; }
		valid = true;
		return *it;
	}

	//! Insert a new element before the current position.
	void insert (const T& t) {
		if ( size_ == 0 ) { list_.push_back (t); cur_ = list_.begin(); }
		else list_.insert (cur_, t);
		++size_;
	}

	//! Removes the element at the current position.
	void erase () {
		if ( size_ == 0 ) return;
		typename std::list<T>::iterator drop = cur_++;
		if ( cur_ == list_.end() ) cur_ = list_.begin();
		list_.erase (drop);
		--size_;
	}

	//! Moves the pointer to the next element in a circular fashion.
	void move () {
		if ( size_ > 0 && ++cur_ == list_.end() ) cur_ = list_.begin(); }

	//! Return the current element.
	/*!
	  If the size is 0, then the return value is not meaningful, and
	  may also crash program execution. We use an assert here since
	  we do not want to use exception, which would be the right thing to do.
	  */
	const T& current () const { assert ( size_ > 0 ); return *cur_; }

	//! Return the current element.
	T& current () { assert ( size_ > 0 ); return *cur_; }
};

#endif // __NS2_WIMAX_CIRCULAR_H
