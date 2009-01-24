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

#ifndef __NS2_WIMAX_HEADER_H
#define __NS2_WIMAX_HEADER_H

#include <packet.h>
#include <wimax_defs.h>

#include <wimsh_header.h>        // :MESH:

//! Fragmentation subheader.
struct WimaxFsh {
public:
	//! Fragmentation states.
	enum { NO_FRAG = 0, LAST_FRAG = 1, FIRST_FRAG = 2, CONT_FRAG = 3 };
private:
	//! Fragmentation state. 2 bits.
	unsigned char state_;
	//! Fragment sequence number (modulo 8). 3 bits.
	unsigned char fsn_;
public:
	//! Build an empty fragmentation subheader.
	WimaxFsh () { state_ = NO_FRAG; }
	//! Get/set the fragmentation state.
	unsigned char& state () { return state_; }
	//! Get/set the sequence number.
	unsigned char& fsn () { return fsn_; }
	//! Return the sequence number space size.
	static unsigned int fsnSize () { return 8; }
	//! Return the size of the fragmentation subheader (in bytes).
	static unsigned int size () { return 1; }
};

//! Generic MAC header.
struct WimaxMacHeader {
private:
	//! CRC indicator. 1 bit.
	bool crc_;
	//! PDU length. 11 bits.
	unsigned int length_;
	//! Fragmentation subheader present. 1 bit.
	bool fragmentation_;
	//! Grant management subheader present 1 bit.
	bool gmsh_;
	//! Mesh CID, 16 bits.         // :MESH:
	WimaxMeshCid meshCid_;         // :MESH:

	// :MESH: {
	//! True if the mesh subheader is present.
	bool mesh_;
	// :MESH: }

public:
	//! Build an empty MAC header.
	WimaxMacHeader () {
		crc_ = false; length_ = 0;
		fragmentation_ = false;
		mesh_ = false;           // :MESH:
		gmsh_ = false; }
	//! Get/set the CRC indicator.
	bool& crc () { return crc_; }
	//! Get/set the PDU length.
	unsigned int& length () { return length_; }
	//! Get/set the fragmentation subheader present flag.
	bool& fragmentation () { return fragmentation_; }
	//! Get/set the grant management subheader present flag.
	bool& gmsh () { return gmsh_; }
	//! Get/set the mesh subheader present flag.      // :MESH:
	bool& mesh () { return mesh_; }                   // :MESH:
	//! Get/set the mesh CID.                         // :MESH:
	WimaxMeshCid& meshCid () { return meshCid_; }     // :MESH:
	//! Return the size of the MAC header (in bytes), including CRC.
	unsigned int size () { return 6 + ( (crc_) ? 4 : 0 ); }
	//! Return the size of a MAC header (in bytes).
	static unsigned int size (bool crc, bool fragmentation = false) {
		return 6 + ( (crc) ? 4 : 0 ) + ( (fragmentation) ? 1 : 0 ); }
};

#endif // __NS2_WIMAX_HEADER_H
