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

#ifndef __NS2_WIMAX_PACKET_H
#define __NS2_WIMAX_PACKET_H

#include <list>
#include <map>

#include <wimax_header.h>
#include <ip.h>

//! Wimax Service Data Unit (SDU). It is an ns2 packet + traversed hops.
/*!
  Note that a WimaxSdu does *not* own its payload. Thus, the copy
  constructor and assignment operator only copy the pointer to the
  payload, which has to be freed only once.
  */
struct WimaxSdu {
public:
	//! Maximum number of hops that can be stored into each SDU.
	enum { MAX_HOPS = 64 };
private:
	//! IP datagram from ns2.
	Packet* ip_;
	//! True if there is payload.
	bool payload_;
	//! Array of the traversed hops.
	WimaxNodeId hops_[MAX_HOPS];
	//! Number of hops traversed.
	unsigned int nhops_;
	//! Timestamp (for statistics collection).
	double timestamp_;
	//! Sequence number (for statistics collection)
	unsigned int packetid_;
public:
	//! Build an empty SDU.
	WimaxSdu () {
		payload_ = false;
		ip_      = 0;
		nhops_   = 0;
	}
	//! Do nothing.
	~WimaxSdu () { }

	//! Return true if there is a payload contained into this SDU.
	bool payload () {
		if ( ip_) return true;
		return false;
	}
	//! Get/set the payload.
	Packet*& ip () { return ip_; }
	//! Deallocate the payload, if any.
	void freePayload () {
		if ( ip_ ) { Packet::free (ip_); ip_ = 0; }
	}
	//! Drop the payload, if any.
	void dropPayload () {
		if ( ip_ ) { Packet::free (ip_); ip_ = 0; }
	}
	//! Copy the payload from another SDU.
	void copyPayload (WimaxSdu* sdu) {
		if ( sdu->ip_ ) ip_ = sdu->ip_->copy();
	}

	//! Return the Flow ID of the payload.
	int flowId () {
		if ( ip_ ) return HDR_IP(ip_)->flowid();
		return 0;
	}

	//! Get/set the timestamp.
	double& timestamp () { return timestamp_; }
	//! Get/set the seq number.
	unsigned int& seqnumber () { return packetid_; }

	//! Add a new hop.
	void addHop (WimaxNodeId hop) { hops_[nhops_++] = hop; }
	//! Return the list of traversed hops.
	void traversedHops (WimaxNodeId*& h, unsigned int& n) {
		h = hops_; n = nhops_; }
	//! Return the number of traversed hops.
	unsigned int nHops () { return nhops_; }
	//! Return the last hop. Return 0 if the number of hops is not stored.
	WimaxNodeId lastHop () { return ( nhops_ > 0 ) ? hops_[nhops_ - 1] : 0; }
	//! Return the hop before the last hop. Useful if trying to find the last hop just after updating the hop list.
	//  Returns UINT_MAX if there is no such hop (to clearly distinguish between hop 0 and no hop).
	WimaxNodeId lastlastHop () { return ( nhops_ > 1 ) ? hops_[nhops_ - 2] : UINT_MAX; }
	//! Check whether a hop has been traversed.
	bool traversed (WimaxNodeId hop) {
		for ( unsigned int i = 0 ; i < nhops_ ; i++ )
			if ( hop == hops_[i] ) return true;
		return false; }

	//! Return the size of the SDU = IP + hops.
	unsigned int size () {
		if ( ip_ ) return HDR_CMN(ip_)->size() + 2 * nhops_;
		return 0;
	}
};

//! Wimax MAC Protocol Data Unit (PDU).
/*!
  It is an SDU + MAC header + fragmentation subheader.
  Instead, the mesh subheader is included directly.       // :MESH:
  Of course, SDUs are not actually fragmented as IRL.
  */
struct WimaxPdu {
public:
	enum { GENERIC };
private:
	//! SDU (or fragment thereof) encapsulated.
	WimaxSdu* sdu_;
	//! Generic MAC header.
	WimaxMacHeader hdr_;
	//! Fragmentation subheader, if present (as specified in the MAC header).
	WimaxFsh fsh_;
	//! Mesh subheader = transmitter NodeId (16 bits).    // :MESH:
	WimaxNodeId nodeId_;                                  // :MESH:
	//! True if the content of this PDU is corrupt.
	bool error_;
	//! Indicates the type of header
	unsigned char type_;
public:
	//! Build an empty PDU.
	WimaxPdu () { error_ = false; type_ = GENERIC; sdu_ = 0; }
	//! Get/set the encapsulated SDU.
	WimaxSdu*& sdu () { return sdu_; }
	//! Get/set the MAC header.
	WimaxMacHeader& hdr () { return hdr_; }
	//! Get/set the header type.
	unsigned char& type () { return type_; }
	//! Get/set the fragmentation subheader.
	WimaxFsh& fsh () { return fsh_; }
	//! Get/set the transmitter NodeId.                     // :MESH:
	WimaxNodeId& nodeId () { return nodeId_; }              // :MESH:
	//! Get/set the error bit.
	bool& error () { return error_; }
	//! Set the PDU size (in bytes), overhead included, with a given payload.
	void size (unsigned int n) {
		hdr_.length() = n + hdr_.size() +
			( ( hdr_.mesh() ) ? meshSubhdrSize() : 0 ) +      // :MESH:
			( ( hdr_.fragmentation() ) ? fsh_.size() : 0 );
	}
	//! Get the PDU size (in bytes), overhead included.
   unsigned int size () {
		return hdr_.length();
   }

	//! Get the PDU size (in bytes), overhead *not* included.
	unsigned int data () {
		return
			hdr_.length()
			- hdr_.size()
			- ( ( hdr_.mesh() ) ? meshSubhdrSize() : 0 )        // :MESH:
         - ( ( hdr_.fragmentation() ) ? fsh_.size() : 0 ); }
	//! Return the mesh subheader size.                       // :MESH:
	static unsigned int meshSubhdrSize () { return 2; }       // :MESH:
	//! Return the minimum size of a MAC PDU (unfragmented, no CRC).
	static unsigned int minSize () { return 6 + 2 + 1; }
};

//! List of PDUs.
typedef std::list<WimaxPdu*> PduList;

#endif // __NS2_WIMAX_PACKET_H
