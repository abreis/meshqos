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

#ifndef __NS2_WIMAX_DEBUG_H
#define __NS2_WIMAX_DEBUG_H

#include <wimax_packet.h>
#include <wimax_circular.h>
#include <wimax_matrix.h>
#include <wimax_defs.h>

#include <stdio.h>
#include <string>
#include <map>

class WimaxSdu;
class WimaxPdu;
class WimshMshDsch;
class WimshMshNcfg;
class WimshMshNent;
class WimshBurst;

//! This macro can be used to hard-disable debug and save computational time.
#define WIMAX_DEBUG

//! Useful debug static class.
/*!
  The 'format' functions return a single-line (non-'\n' terminated) sequence.
  
  On the other hand, the 'print' functions print a longer description
  on a given output stream file. An optional argument allows the caller
  to specify a string that has to be added at the beginning of each line.
  */
class WimaxDebug : public TclObject {
	enum { NBUFS = 10 };
	enum { BUFSIZE = 1024 };
	//! Static buffer returned by format functions.
	static char buf_[NBUFS][BUFSIZE];
	//! Current buffer.
	static unsigned int cur_;
	//! True if debug is enabled.
	static bool enabled_;
	//! Map of function names to flag (true means this function is traced).
	static std::map<std::string, bool> trace_;

public:
	//! Do nothing.
	WimaxDebug () { }
	//! Do nothing.
	virtual ~WimaxDebug () { }
	//! Print a line with SDU information.
	static char* format (WimaxSdu* sdu);
	//! Print a line with PDU information.
	static char* format (WimaxPdu* pdu);
	//! Print a line with ns2 packet information.
	static char* format (Packet* pkt);
	//! Convert a burst profile ID into a human-readable string.
	static char* format (wimax::BurstProfile bp);
	//! Convert a burst type into a human-readable string.
	static char* format (wimax::BurstType);

	//! Print the content of a WimshBurst.
	static void print (WimshBurst* burst, FILE* os, const char* hdr = "\t");
	//! Print the content of a DL-MAP message.
	//! Print a circular list of unsigned integers.
	static void print (CircularList<unsigned int>& list,
			FILE* os, const char* hdr = "\t");

	// :MESH: {
	//! Print the content of an MSH-DSCH message.
	static void print (WimshMshDsch* dsch, FILE* os, const char* hdr = "\t");
	//! Print the content of an MSH-NCFG message.
	static void print (WimshMshNcfg* ncfg, FILE* os, const char* hdr = "\t");
	//! Print the content of an MSH-NENT message.
	static void print (WimshMshNent* nent, FILE* os, const char* hdr = "\t");
	//! Print a circular list of LinkId's.
	static void print (CircularList<wimax::LinkId>& list,
			FILE* os, const char* hdr = "\t");
	// :MESH: }

	//! Get/set the enabled flag.
	static bool& enabled () { return enabled_; }

#ifdef WIMAX_DEBUG

	//! Return true if this function is traced and debug is enabled.
	static bool trace (const char* f) {
		return ( enabled_ &&
				   trace_.count(std::string(f)) == 1 &&
				   trace_[std::string(f)] == true ) ? true : false; }

	//! Set the trace true/false status of a function name.
	static void trace (const char* f, bool status) {
		trace_[std::string(f)] = status; }
#else

	static bool trace (const char* f) { return false; }
	static void trace (const char* f, bool status) { }

#endif // WIMAX_DEBUG

protected:
	//! Tcl interface.
	virtual int command (int argc, const char*const* argv);
};

#endif // __NS2_WIMAX_DEBUG_H
