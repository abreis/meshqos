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

#include <wimax_debug.h>
#include <wimsh_packet.h>   // :MESH:

#include <packet.h>
#include <ip.h>

static class WimaxDebugClass : public TclClass {
public:
   WimaxDebugClass() : TclClass("WimaxDebug") {}
   TclObject* create(int, const char*const*) {
      return (new WimaxDebug);
   }
} class_wimax_debug;

char WimaxDebug::buf_[NBUFS][BUFSIZE];
unsigned int WimaxDebug::cur_ = 0;
bool WimaxDebug::enabled_ = true;
std::map<std::string, bool> WimaxDebug::trace_;

int 
WimaxDebug::command (int argc, const char*const* argv)
{
	if ( argc == 2 && strcmp (argv[1], "enable") == 0 ) {
		enabled_ = true;
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[1], "disable") == 0 ) {
		enabled_ = false;
		return TCL_OK;
	} else if ( argc == 4 && strcmp (argv[1], "trace") == 0 ) {
		bool status;

		if ( strcmp (argv[3], "on") == 0 ) status = true;
		else if ( strcmp (argv[3], "off") == 0 ) status = false;
		else {
			fprintf (stderr, "the trace status of function '%s' must be "
					"either 'on' or 'off', not '%s'\n", argv[2], argv[3]);
			return TCL_ERROR;
		}

		trace (argv[2], status);
		return TCL_OK;
	}

	return TCL_ERROR;
}

char*
WimaxDebug::format (Packet* pkt)
{
	char* b = buf_[cur_];
	cur_ = ( cur_ + 1 == NBUFS ) ? 0 : ( cur_ + 1 );

	snprintf (b, BUFSIZE, "IP fid %d size %d src %d dst %d uid %d",
			HDR_IP(pkt)->flowid(), HDR_CMN(pkt)->size(),
			HDR_IP(pkt)->saddr(), HDR_IP(pkt)->daddr(), HDR_CMN(pkt)->uid());
	return b;
}

char*
WimaxDebug::format (WimaxSdu* sdu)
{
	char* b = buf_[cur_];
	cur_ = ( cur_ + 1 == NBUFS ) ? 0 : ( cur_ + 1 );

	WimaxNodeId* hops;
	unsigned int nhops;
	sdu->traversedHops (hops, nhops);

	int n;

	if ( sdu->ip() ) {
		n = snprintf (b, BUFSIZE, "%s SDU", format (sdu->ip()));
		for ( unsigned int i = 0 ; i < nhops ; i++ ) {
			n += snprintf (b + n, BUFSIZE - n, " hop %d", hops[i]);
		}
	} else {
		snprintf (b, BUFSIZE, "unknown SDU payload");
	}

	return b;
}

char*
WimaxDebug::format (WimaxPdu* pdu)
{
	char* b = buf_[cur_];
	cur_ = ( cur_ + 1 == NBUFS ) ? 0 : ( cur_ + 1 );

	int n;

	if (pdu->type() == WimaxPdu::GENERIC){
			n = snprintf (b, BUFSIZE,
					"%s PDU src %d dst %d err %d size %d crc %d prio %d drop %d",
					format(pdu->sdu()),
					pdu->nodeId(), pdu->hdr().meshCid().dst(), pdu->error(),
					pdu->size(), pdu->hdr().crc(), pdu->hdr().meshCid().priority(),
					pdu->hdr().meshCid().drop());

		if ( pdu->hdr().fragmentation() )
			snprintf (b + n, BUFSIZE - n,
					" frag %s seq %d",
					( pdu->fsh().state() == WimaxFsh::NO_FRAG ) ? "none " :
					( pdu->fsh().state() == WimaxFsh::LAST_FRAG ) ? "last " :
					( pdu->fsh().state() == WimaxFsh::FIRST_FRAG ) ? "first" :
					( pdu->fsh().state() == WimaxFsh::CONT_FRAG ) ? "cont " : "unkn ",
					pdu->fsh().fsn());
	} else {
		n = snprintf (b, BUFSIZE, "UNKNOWN PDU type '%d'", pdu->type());
	}
	return b;
}

char*
WimaxDebug::format (wimax::BurstProfile bp)
{
	char* b = buf_[cur_];
	cur_ = ( cur_ + 1 == NBUFS ) ? 0 : ( cur_ + 1 );

	snprintf (b, BUFSIZE, "%s",
			( bp == wimax::QPSK_1_2 ) ? "QPSK-1/2" :
			( bp == wimax::QPSK_3_4 ) ? "QPSK-3/4" :
			( bp == wimax::QAM16_1_2 ) ? "16QAM-1/2" :
			( bp == wimax::QAM16_3_4 ) ? "16QAM-3/4" :
			( bp == wimax::QAM64_2_3 ) ? "64QAM-2/3" :
			( bp == wimax::QAM64_3_4 ) ? "64QAM-3/4" : 
			"UNKNOWN" );

	return b;
}

char*
WimaxDebug::format (wimax::BurstType type)
{
	char* b = buf_[cur_];
	cur_ = ( cur_ + 1 == NBUFS ) ? 0 : ( cur_ + 1 );

	snprintf (b, BUFSIZE, "%s",
			( type == wimax::DATA ) ? "DATA" :          // :MESH:
			( type == wimax::MSHDSCH ) ? "MSH-DSCH" :   // :MESH:
			( type == wimax::MSHNCFG ) ? "MSH-NCFG" :   // :MESH:
			( type == wimax::MSHNENT ) ? "MSH-NENT" :   // :MESH:
			"UNKNOWN" );

	return b;
}

// :MESH: {
void
WimaxDebug::print (WimshBurst* burst, FILE* os, const char* hdr)
{
	// Print common fields.
	fprintf (os, "%sBurst type %s size %u error %u txtime %f profile %s "
			"source %u\n",
			hdr, format(burst->type()), burst->size(),
			burst->error(), burst->txtime(), format (burst->profile()),
			burst->source());

	// Demux on the burst type.
	if ( burst->type() == wimax::MSHDSCH )
		print ( burst->mshDsch(), os, hdr );
	else if ( burst->type() == wimax::MSHNCFG )
		print ( burst->mshNcfg(), os, hdr );
	else if ( burst->type() == wimax::MSHNENT )
		print ( burst->mshNent(), os, hdr );
	else if ( burst->type() == wimax::DATA ) {
		std::list<WimaxPdu*>& pdus = burst->pdus();
		std::list<WimaxPdu*>::iterator it;
		for ( it = pdus.begin() ; it != pdus.end() ; ++it ) {
			fprintf (os, "%s%s\n", hdr, format (*it));
		}
	} else
		fprintf (os, "%sUNKNOWN burst type\n", hdr);
}


void
WimaxDebug::print (WimshMshDsch* dsch, FILE* os, const char* hdr)
{
	// fixed fields
	fprintf (os, "%sMSH-DSCH size %d (remaining %d)\n",
			hdr, dsch->size(), dsch->remaining());

	// coordination
	std::list<WimshMshDsch::NghIE>& ngh = dsch->ngh();
	std::list<WimshMshDsch::NghIE>::iterator nghIt;
	fprintf (os, "%sCOORDINATION (%d)\n"
			"%snode  %3d mx %3d exp %3d (this node)\n",
			hdr, ngh.size(), hdr, dsch->myself().nodeId_,
			dsch->myself().nextXmtMx_, dsch->myself().xmtHoldoffExponent_);
	for ( nghIt = ngh.begin() ; nghIt != ngh.end() ; nghIt++ )
		fprintf (os, "%snode  %3d mx %3d exp %3d\n",
				hdr, nghIt->nodeId_, nghIt->nextXmtMx_, nghIt->xmtHoldoffExponent_);

	// availabilities
	std::list<WimshMshDsch::AvlIE>& avl = dsch->avl();
	std::list<WimshMshDsch::AvlIE>::iterator avlIt;
	fprintf (os, "%sAVAILABILITIES (%d)\n", hdr, avl.size());
	for ( avlIt = avl.begin() ; avlIt != avl.end() ; avlIt++ )
		fprintf (os,"%sframe %3d start %3d range %3d dir %s pers %1d chan %2d\n",
				hdr, avlIt->frame_, avlIt->start_, avlIt->range_,
				( avlIt->direction_ == 0 ) ? "NO" :
				( avlIt->direction_ == 1 ) ? "TX" :
				( avlIt->direction_ == 2 ) ? "RX" :
				( avlIt->direction_ == 3 ) ? "OK" : "XX",
				avlIt->persistence_, avlIt->channel_);

	// requests
	std::list<WimshMshDsch::ReqIE>& req = dsch->req();
	std::list<WimshMshDsch::ReqIE>::iterator reqIt;
	fprintf (os, "%sREQUESTS (%d)\n", hdr, req.size());
	for ( reqIt = req.begin() ; reqIt != req.end() ; reqIt++ )
		fprintf (os, "%snode  %3d level %3d pers %1d\n",
				hdr, reqIt->nodeId_, reqIt->level_, reqIt->persistence_);

	// grants
	std::list<WimshMshDsch::GntIE>& gnt = dsch->gnt();
	std::list<WimshMshDsch::GntIE>::iterator gntIt;
	fprintf (os, "%sGRANTS/CONFIRMATIONS (%d)\n", hdr, gnt.size());
	for ( gntIt = gnt.begin() ; gntIt != gnt.end() ; gntIt++ )
		fprintf (os, "%snode  %3d frame %3d start %3d range %3d dir %s "
				"pers %1d chan %2d\n",
				hdr, gntIt->nodeId_, gntIt->frame_,
				gntIt->start_, gntIt->range_,
				( gntIt->fromRequester_ == true ) ? "CNF" : "GNT",
				gntIt->persistence_, gntIt->channel_);
}

void
WimaxDebug::print (WimshMshNcfg* ncfg, FILE* os, const char* hdr)
{
	// fixed fields
	fprintf (os, "%sMSH-NCFG size %d (remaining %d)\n",
			hdr, ncfg->size(), ncfg->remaining());
}

void
WimaxDebug::print (WimshMshNent* nent, FILE* os, const char* hdr)
{
	// fixed fields
	fprintf (os, "%sMSH-NENT size %d (remaining %d)\n",
			hdr, nent->size(), nent->remaining());
}

void
WimaxDebug::print (CircularList<wimax::LinkId>& list,
		FILE* os, const char* hdr)
{
	fprintf (os, "%ssize %d queue", hdr, list.size_);
	if ( list.size_ == 0 ) {
		fprintf (os, " empty\n");
		return;
	}
	std::list<wimax::LinkId>::iterator it;
	for ( it = list.list_.begin() ; it != list.list_.end() ; ++it ) {
		fprintf (os, " %d,%s", it->ndx_, (it->dir_ == wimax::IN) ? "in" : "out");
	}
	fprintf (os, "\n");
}
// :MESH: }

void
WimaxDebug::print (CircularList<unsigned int>& list,
		FILE* os, const char* hdr)
{
	fprintf (os, "%ssize %d queue", hdr, list.size_);
	if ( list.size_ == 0 ) {
		fprintf (os, " empty\n");
		return;
	}
	std::list<unsigned int>::iterator it;
	for ( it = list.list_.begin() ; it != list.list_.end() ; ++it ) {
		fprintf (os, " %d", *it);
	}
	fprintf (os, "\n");
}
