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

#ifndef __NS2_WIMAX_BUFFERS_H
#define __NS2_WIMAX_BUFFERS_H

#include <wimax_common.h>
#include <wimax_packet.h>

#include <vector>

/*
 *
 * class WimaxReassemblyBuffer
 *
 */

//! Reassemble PDU fragments into an SDU.
/*!
  Out of order fragments cause the SDU/PDU to be discarded.
  Eigth parallel reassembly buffers exist, so that multiple fragments
  of SDUs of the same connection/node can be recovered.
  */
class WimaxReassemblyBuffer {
	//! Last PDU received.
	std::vector<WimaxPdu*> pdu_;
public:
	//! Create an empty reassembly buffer.
	WimaxReassemblyBuffer ();
	//! Do nothing.
	~WimaxReassemblyBuffer () { }

	//! Add a PDU to the buffer. Return an SDU, if complete.
	WimaxSdu* addPdu (WimaxPdu* pdu);
};

#endif // __NS2_WIMAX_BUFFERS_H
