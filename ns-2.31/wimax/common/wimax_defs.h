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

#ifndef __NS2_WIMAX_DEFS_H
#define __NS2_WIMAX_DEFS_H

#include <object.h>
#include <stdio.h>

//! For WiMAX NodeID we use an unsigned int (not an int as for ns2 node IDs).
typedef unsigned int WimaxNodeId;

namespace wimax {

//! Broadcast CID.
enum { BROADCAST = 0xFFFFFFFF };

//! PHY status on this channel (transmit, receive, or none).
enum ChannelStatus { TX, RX, NONE };

//! PDU burst type.
enum BurstType { DATA, MSHNCFG, MSHNENT, MSHDSCH };

// :MESH: {
//! Link direction (input or output).
enum LinkDirection { IN, OUT };
// :MESH: }

// :MESH: {
//! Drop precedence for mesh mode.
enum DropPrecedence { DROP_UNSPECIFIED, DROP_HIGH, DROP_MIDDLE, DROP_LOW };
// :MESH: }

//! Physical burst profiles.
enum BurstProfile {
	QPSK_1_2,  QPSK_3_4,
	QAM16_1_2, QAM16_3_4,
	QAM64_2_3, QAM64_3_4,
	N_BURST_PROFILES         // this is the number of *data* burst profiles
};

// :MESH: {

//! Input/output link identifier in the round robin active list.
struct LinkId {
	//! Index of the link's neighbor.
	unsigned int ndx_;
	//! Link's direction.
	wimax::LinkDirection dir_;
	//! Default constructor.
	LinkId (unsigned int ndx = 0, wimax::LinkDirection dir = wimax::IN)
		: ndx_ (ndx), dir_ (dir) { }
	//! Return false if either ndx_ or dir_ are different.
	bool operator!= (const LinkId& x) {
		return ( x.ndx_ != ndx_ || x.dir_ != dir_ ); }
};

//! Next-hop and number of hops towards a destination

struct NextHopInfo {
	//! Next hop Id
	WimaxNodeId nextHop_;
	//! Number of hops towards a destination
	unsigned int nHops_;

	//! Constructor
	NextHopInfo( WimaxNodeId nextHop = 0, unsigned int nHops = 0 ) {
		nextHop_ = nextHop; nHops_ = nHops; }
};

// :MESH: }

}

#endif // __NS2_WIMAX_DEFS_H
