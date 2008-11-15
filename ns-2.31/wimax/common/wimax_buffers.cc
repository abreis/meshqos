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

#include <wimax_buffers.h>

/*
 *
 * class WimaxReassemblyBuffer
 *
 */

WimaxReassemblyBuffer::WimaxReassemblyBuffer ()
{
	// Resize the vector to the number of different SDUs that can
	// be fragmented at the same time. With standard fragmentation
	// subheader, this number is eigth.
	pdu_.resize (WimaxFsh::fsnSize());
	for ( unsigned int i = 0 ; i < WimaxFsh::fsnSize() ; i++ ) pdu_[i] = 0;
}

WimaxSdu*
WimaxReassemblyBuffer::addPdu (WimaxPdu* newpdu)
{
	// Fragment Sequency Number of the incoming PDU.
	// This is used to demultiplex the correct PDU buffer.
	const unsigned char fsn = newpdu->fsh().fsn(); // alias

	// If this is not a fragment, then we immediately return a new SDU.
	if ( newpdu->hdr().fragmentation() == false )
		return newpdu->sdu();

	// If must never happen that the sequence number is greater than
	// or equal to the space size, in which case we just abort since
	// this would cause overflow of the pdu_ vector.
	if ( fsn >= WimaxFsh::fsnSize() ) abort ();
	
	// If this is a new fragment, let us store it for later.
	if ( newpdu->fsh().state() == WimaxFsh::FIRST_FRAG ) {
		// The stored PDU is dropped, if any.
		if ( pdu_[fsn] ) {
			pdu_[fsn]->sdu()->dropPayload();
			delete pdu_[fsn]->sdu();
			delete pdu_[fsn];
		}
		pdu_[fsn] = new WimaxPdu (*newpdu);
		// fall-through

	} else if ( newpdu->fsh().state() == WimaxFsh::LAST_FRAG ) {
		//
		// :XXX:
		// The size should be checked to verify that all fragments
		// have been received.
		//
		if ( pdu_[fsn] ) {
			pdu_[fsn]->sdu()->freePayload();
			delete pdu_[fsn]->sdu();
			delete pdu_[fsn];
		}
		pdu_[fsn] = 0;
		return newpdu->sdu();

	} else {  // newpdu->fsh.state() == WimaxFsh::CONT_FRAG
		if ( pdu_[fsn] ) {
			pdu_[fsn]->sdu()->freePayload();
			delete pdu_[fsn]->sdu();
			delete pdu_[fsn];
		}
		pdu_[fsn] = new WimaxPdu (*newpdu);
		// fall-through

	}

	// Do not return a complete SDU.
	return 0;
}
