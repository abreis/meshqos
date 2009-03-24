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

#include <wimsh_scheduler_frr.h>

#include <wimsh_mac.h>
#include <wimsh_packet.h>
#include <wimsh_bwmanager.h>

#include <stat.h>
#include <ip.h>

WimshSchedulerFairRR::WimshSchedulerFairRR (WimshMac* m) :
	WimshScheduler (m), timer_(this)
{
	roundDuration_     = 0;
	bufferSharingMode_ = SHARED;
	interval_          = 0;
	for ( unsigned int i = 0 ; i < WimaxMeshCid::MAX_PRIO ; i++ )
		prioWeights_[i] = 1.0;
}

int
WimshSchedulerFairRR::command (int argc, const char * const* argv)
{
	if ( argc == 2 && strcmp (argv[0], "round-duration") == 0 ) {
		if ( atoi (argv[1]) <= 0 ) {
			fprintf (stderr, "Invalid round duration '%s'. "
					"Choose a positive number, in bytes\n", argv[1]);
			return TCL_ERROR;
		}
		roundDuration_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "buffer-sharing") == 0 ) {
		if ( strcmp (argv[1], "shared") == 0 ) {
			bufferSharingMode_ = SHARED;
		} else if ( strcmp (argv[1], "per-link") == 0 ) {
			bufferSharingMode_ = PER_LINK;
		} else if ( strcmp (argv[1], "per-flow") == 0 ) {
			bufferSharingMode_ = PER_FLOW;
		} else {
			fprintf (stderr, "Invalid buffer sharing strategy '%s'. "
					"Choose 'shared' or 'per-link' or 'per-flow'\n", argv[1]);
			return TCL_ERROR;
		}
		return TCL_OK;
   } else if ( argc == 2 && strcmp (argv[0], "weight-timeout") == 0 ) {
      interval_ = atof (argv[1]);
      return TCL_OK;
   } else if ( argc == 3 && strcmp (argv[0], "prio-weight") == 0 ) {
      unsigned int i = atoi (argv[1]);
      double x = atof (argv[2]);
      if ( i >= WimaxMeshCid::MAX_PRIO ) {
         fprintf (stderr, "priority '%d' exceeds the maximum value '%d'\n",
               i, WimaxMeshCid::MAX_PRIO - 1 );
         return TCL_ERROR;
      }
      if ( x <= 0 ) {
         fprintf (stderr,
               "priority weight must be >= 0 ('%f' not allowed)\n", x);
         return TCL_ERROR;
      }
      prioWeights_[i] = x;
      return TCL_OK;
	}

	return WimshScheduler::command (argc, argv);
}

void
WimshSchedulerFairRR::initialize ()
{
	// number of neighbors for this node
	const unsigned int neighbors = mac_->nneighs();

	// resize the vector of links and unfinished vectors
	cbr_.resize (neighbors);
	link_.resize (neighbors);
	unfinishedRound_.resize (neighbors);

	// resize the 2D vectors for 4 services
	for ( unsigned int ngh = 0 ; ngh < neighbors ; ngh++ ) {
		cbr_[ngh].resize (4);
		link_[ngh].resize (4);
		unfinishedRound_[ngh].resize (4);
	}

	// start the timer to remove stale flows
	if ( interval_ > 0 ) timer_.start ( interval_ );
}

void
WimshSchedulerFairRR::addPdu (WimaxPdu* pdu)
{
	if ( WimaxDebug::trace("WSCH::addPdu") ) fprintf (stderr,
			"%.9f WSCH::addPdu     [%d] %s",
			NOW, mac_->nodeId(), WimaxDebug::format(pdu));

	// index used to identify the next-hop neighbor node
	const unsigned int ndx = mac_->neigh2ndx (pdu->hdr().meshCid().dst());

	// priority of this PDU
	const unsigned char prio = pdu->hdr().meshCid().priority();

	// map IP PRIO field to service class at the mac layer
	const unsigned char s = WimshMshDsch::prio2serv(prio);

	// if the size of this PDU overflows the buffer size, drop the PDU/SDU/IP
	if ( ( bufferSharingMode_ == SHARED && bufSize_ + pdu->size() > maxBufSize_ ) ||
			( bufferSharingMode_ == PER_LINK && link_[ndx][s].size_ + pdu->size() > maxBufSize_) ) {
		drop (pdu);
		if ( WimaxDebug::trace("WSCH::addPdu") ) fprintf (stderr, " dropped\n");
		return;
	}

	// check if there are already PDUs buffered from this traffic flow
	// to do so, we create a new FlowDesc with empty queue, which is
	// used only to this purpose. If there are no buffered PDUs that
	// belong to this traffic flow, then this descriptor is added
	// to the round-robin list

	// get the source/destination addresses of this PDU
	const WimaxNodeId src = (WimaxNodeId) HDR_IP(pdu->sdu()->ip())->saddr();
	const WimaxNodeId dst = (WimaxNodeId) HDR_IP(pdu->sdu()->ip())->daddr();

	// temporary flow descriptor used to search into the active-list
	FlowDesc tmp (src, dst, prio);

	// flag used to check whether such a flow already exists
	bool valid;
	FlowDesc& desc = link_[ndx][s].rr_.find (tmp, valid);

	// check for per-flow buffer overflow
	if ( bufferSharingMode_ == PER_FLOW &&
		  desc.size_ + pdu->size() > maxBufSize_ ) {
		drop (pdu);	// TODO: 2hop+ throughput issue begins here
		if ( WimaxDebug::trace("WSCH::addPdu") ) fprintf (stderr, " dropped\n");
		return;
	}

	// add the PDU to the flow descriptor
	desc.queue_.push (pdu);

	// update the cumulative/flow/link buffer occupancy
	// this includes MAC overhead (header/crc), but not the fragmentation
	// subheader. In fact, at the moment we do not know whether it will
	// be added by the fragmentation buffer or not

	desc.size_ += pdu->size();        // per flow
	link_[ndx][s].size_ += pdu->size();  // per link/service pair
	bufSize_ += pdu->size();          // shared

	if ( WimaxDebug::trace("WSCH::addPdu") ) fprintf (stderr,
			" buffsize %d\n",
			link_[ndx][s].size_);

	Stat::put ("wimsh_bufsize_mac_a", mac_->index(), bufSize_ );
	Stat::put ("wimsh_bufsize_mac_d", mac_->index(), bufSize_ );

	// add the flow descriptor into the active list, is not already there
	// if this is the case, then the weights should be recomputed, as well
	if ( ! valid ) {
		link_[ndx][s].rr_.insert (desc);
		recompute (link_[ndx][s].rr_);
	}

	// indicate the updated backlog to the bandwidth manager
	mac_->bwmanager()->backlog (
			desc.src_,                        // src node
			desc.dst_,                        // dst node
			pdu->hdr().meshCid().priority(),  // priority
			pdu->hdr().meshCid().dst(),       // next-hop
			pdu->size());                     // bytes

	// call searchTXslot to handle uncoordinated message to this neighbour
	// confirm that wasn't alredy called for bwmanager
	// only considered for rtPS traffic
	if ( s == wimax::RTPS && (mac_->bwmanager()->nextFrame_rtPS(ndx) + 1) < mac_->frame() )
		mac_->bwmanager()->searchTXslot(ndx, 0);

	// add this PDU to flow rate statistics and recompute
	recomputeCBR (pdu);
}

void
WimshSchedulerFairRR::schedule (WimshFragmentationBuffer& frag, WimaxNodeId dst, unsigned int s)
{
	// get the link index
	unsigned int ndx = mac_->neigh2ndx(dst);

	// print some debug information
	if ( WimaxDebug::trace("WSCH::schedule") ) {
		fprintf (stderr,
			"%.9f WSCH::schedule   [%d] dst %d remaining %d",
			NOW, mac_->nodeId(), dst, frag.size());

		CircularList<FlowDesc>& rr = link_[ndx][s].rr_;
		fprintf (stderr, " backlog %d", link_[ndx][s].size_);
		if ( rr.size() == 0 ) {
			fprintf (stderr, " qsize 0");
		} else {
			fprintf (stderr, " qsize %d", rr.size());
			for ( unsigned int i = 0 ; i < rr.size() ; i++ ) {
				FlowDesc& flow = rr.current();
				fprintf (stderr, " (%d,%d,%d; %d; %d,%d)",
						flow.src_, flow.dst_, flow.prio_,
						flow.size_,
						flow.quantum_, flow.deficit_);
				rr.move ();
			}
		}
		fprintf (stderr, "\n");
	}

	// true until there is spare room into the burst
	bool spare = true;

	// if there is an unfinished round, then the latter must be terminated
	// before new flows get scheduled, even though the priority of the
	// current flow is lower than that of other active flows
	// however, then we do not update the deficit counter with a new quantum

	if ( unfinishedRound_[ndx][s] ) spare = serve (frag, ndx, s, true);

	// get the round-robin list
	CircularList<FlowDesc>& rr = link_[ndx][s].rr_;

	// schedule PDUs directed to the specified neighbor on a DRR fashion
	// until there is room into the fragmentation buffer
	// and the output queue towards that node is backlogged
	while ( ! rr.empty() && spare ) spare = serve (frag, ndx, s, false);

}

void
WimshSchedulerFairRR::drop (WimaxPdu* pdu)
{
	pdu->sdu()->freePayload();
	delete pdu->sdu();
	delete pdu;

	Stat::put ("wimsh_drop_overflow", mac_->index(), 1.0);
}

bool
WimshSchedulerFairRR::serve (WimshFragmentationBuffer& frag,
		unsigned int ndx, unsigned int s, bool unfinished)
{
	// get the current round-robin list
	CircularList<FlowDesc>& rr = link_[ndx][s].rr_;

	// get the current traffic flow
	FlowDesc& flow = rr.current();

	// update the deficit counter, unless unfinished
	if ( ! unfinished ) {
		flow.deficit_ += flow.quantum_;
		flow.deficit_ =
			( flow.deficit_ > flow.size_ ) ? flow.size_ : flow.deficit_;
	}

	// becomes false if there is no more capacity available
	bool spare = true;

	// try to grant up to flow.deficit_ bytes from this queue
	while ( spare && flow.deficit_ > 0 ) {

		// get the head-of-line PDU
		WimaxPdu* pdu = flow.queue_.front ();

		// check whether this PDU fits into the remaining deficit
		// if not, then it is not possible to transmit it
		if ( pdu->size() > flow.deficit_ ) break;

		// otherwise, remove the PDU from queue
		// note that it may happen that this PDU will not be entirely
		// transmitted by the fragmentation buffer. This is not an issue
		// from the point of view of the scheduler, since the MAC is
		// responsible for ensuring that pending fragments are transmitted
		// in order before other PDUs have any chance to interfere
		flow.queue_.pop ();

		// update the buffer occupancies
		flow.size_ -= pdu->size();			// flow
		link_[ndx][s].size_ -= pdu->size();	// link
		bufSize_ -= pdu->size();			// MAC

		Stat::put ("wimsh_bufsize_mac_a", mac_->index(), bufSize_ );
		Stat::put ("wimsh_bufsize_mac_d", mac_->index(), bufSize_ );

		// update the deficit counter
////////////////////////////////////////////////////////////////////////////////
// :XXX: -= sdu->size()
		flow.deficit_ -= pdu->size();
////////////////////////////////////////////////////////////////////////////////

		// add the PDU to the fragmentation buffer
		spare = frag.addPdu (pdu, s);

		if ( WimaxDebug::trace("WSCH::serve") ) fprintf (stderr,
					"%.9f WSCH::serve      [%d] link %i serv %d buffsize %d pdu-size %d spare %d\n",
					NOW, mac_->nodeId(), ndx, s, link_[ndx][s].size_, pdu->size(), spare);
	}

	// if this round terminated because there is no more spare room
	// in the burst of PDUs, then we neither move the round-robin pointer
	// nor we remove the current element. In this way, next scheduling
	// phase will restart exactly from where we leave now

	if ( ! spare ) {
		unfinishedRound_[ndx][s] = true;
		return spare;
	}

	unfinishedRound_[ndx][s] = false;

	// if the current element does not have any more PDUs, remove it
	if ( flow.size_ == 0 )
		rr.erase ();

	// else, move the round-robin pointer to the next element in the list
	else
		rr.move ();

	return spare;
}

void
WimshSchedulerFairRR::recompute (CircularList<FlowDesc>& rr)
{
	// Number of elements in the round-robin list.
	const unsigned int N = rr.size();
	// Sum of the priority weights of all the elements.
	double sum = 0;

	// Compute the sum of the priority weights.
	for ( unsigned int i = 0 ; i < N ; i++ ) {
		sum += prioWeights_[rr.current().prio_];
		rr.move ();
	}

	// Update the quanta. The minimum quantum value is 1 byte.
	for ( unsigned int i = 0 ; i < N ; i++ ) {
		unsigned int quantum = (unsigned int) ( roundDuration_ *
				( prioWeights_[rr.current().prio_] / (double)sum ) );
		rr.current().quantum_ = ( quantum > 0 ) ? quantum : 1;
		rr.move ();
	}
}

void
WimshSchedulerFairRR::recomputeCBR (WimaxPdu* pdu)
{
	// index used to identify the next-hop neighbor node
	const unsigned int ndx = mac_->neigh2ndx (pdu->hdr().meshCid().dst());
	// priority of this PDU
	const unsigned char prio = pdu->hdr().meshCid().priority();
	// map IP PRIO field to service class at the mac layer
	const unsigned char s = WimshMshDsch::prio2serv(prio);
	// size of this PDU
	const unsigned int bytes = pdu->size();
	// node from where this pdu arrived (MAX_UINT if originating here)
	const unsigned int lastHop = mac_->neigh2ndx( pdu->sdu()->lastlastHop() );

	if ( cbr_[ndx][s].pkt_ == 0 ) {
		// first run of recomputeCBR
		cbr_[ndx][s].startime_ = NOW;

		// clear and resize variable size structures
		cbr_[ndx][s].fwdquocient_.clear();
		cbr_[ndx][s].fwdquocient_.resize(mac_->nneighs(),0);
		cbr_[ndx][s].fwdbytes_.clear();
		cbr_[ndx][s].fwdbytes_.resize(mac_->nneighs(),0);

		if ( WimaxDebug::trace("WSCH::recomputeCBR") ) fprintf (stderr,
					"%.9f WSCH::recompCBR  [%d] recomputeCBR start ndx %d serv %d pkt %d bytes %d startime %.9f\n", NOW, mac_->nodeId(),
					ndx, s, cbr_[ndx][s].pkt_, cbr_[ndx][s].bytes_, cbr_[ndx][s].startime_);

	} else {
		// recompute statistics
		if ( NOW - cbr_[ndx][s].startime_ != 0 ) { // Prevent a divide by zero
			// quocient = (bytes*8 / elapsed time) [bps]
			cbr_[ndx][s].quocient_ = (unsigned int) ( cbr_[ndx][s].bytes_ * 8 / (NOW - cbr_[ndx][s].startime_ ) );

			// if this pdu is not directed to a neighbor, recompute extquocient_
			if( pdu->hdr().meshCid().dst() != (unsigned) HDR_IP( pdu->sdu()->ip() )->daddr() )
				cbr_[ndx][s].extquocient_ = (unsigned) ( cbr_[ndx][s].extbytes_ * 8 / (NOW - cbr_[ndx][s].startime_ ) );

			// if this pdu is only being forwarded by this node, recompute fwdquocient_[srcndx]
			if( pdu->sdu()->lastlastHop() != mac_->nodeId() && pdu->sdu()->lastlastHop() != UINT_MAX )
				cbr_[ndx][s].fwdquocient_[lastHop] = (unsigned) ( cbr_[ndx][s].fwdbytes_[lastHop] * 8 / (NOW - cbr_[ndx][s].startime_ ) );

		} //else { cbr_[ndx][s].quocient_ = 0; };

		if ( WimaxDebug::trace("WSCH::recomputeCBR") ) {
			fprintf (stderr,
					"%.9f WSCH::recompCBR  [%d] ndx %d serv %d pkt %d bytes %d startime %.9f deltatime %.9f frame %d estimate %d extestimate %d\n", NOW, mac_->nodeId(),
					ndx, s, cbr_[ndx][s].pkt_, cbr_[ndx][s].bytes_, cbr_[ndx][s].startime_, (NOW - cbr_[ndx][s].startime_), mac_->frame(), cbr_[ndx][s].quocient_, cbr_[ndx][s].extquocient_);

			for(unsigned i=0; i < mac_->nneighs() ; i++)
				if( ndx != i && cbr_[ndx][s].fwdquocient_[i] != 0 )
					fprintf (stderr,
						"\t src %d dst %d sndx %d dndx %d serv %d fwdquocient %d\n",
						mac_->ndx2neigh(i), mac_->ndx2neigh(ndx), i, ndx, s, cbr_[ndx][s].fwdquocient_[i]);
		}
	}

	cbr_[ndx][s].pkt_++; // Increase packet count
	cbr_[ndx][s].bytes_ += bytes; // Update byte count
	if( pdu->hdr().meshCid().dst() != (unsigned) HDR_IP(pdu->sdu()->ip())->daddr() )
		cbr_[ndx][s].extbytes_ += bytes; // update byte count of intermediate traffic
	if( pdu->sdu()->lastlastHop() != mac_->nodeId() )
		cbr_[ndx][s].fwdbytes_[lastHop] += bytes;	// update byte count of forwarding traffic

	if (cbr_[ndx][s].pkt_ >= 100) { // Reset counts every 100 packets
		cbr_[ndx][s].pkt_ = 0;
		cbr_[ndx][s].bytes_ = 0;
		cbr_[ndx][s].extbytes_ = 0;
		cbr_[ndx][s].quocient_ = 0;
		cbr_[ndx][s].extquocient_ = 0;
		//cbr_[ndx][s].starttime is reset on the next iteration due to (pkt_ = 0)
		//fwdquocient_ and fwdbytes_ likewise
	}
}

void
WimshSchedulerFairRR::handle ()
{
	if ( WimaxDebug::trace("WSCH::handle") ) fprintf (stderr,
			"%.9f WSCH::handle     [%d]\n", NOW, mac_->nodeId());

	// if the user specified a zero or negative value for the interval
	// then he/she wants the weights never to expire => return immediately
	// without restarting the timer, hence this function will not be
	// called again
	if ( interval_ <= 0 ) return;

	// remove stale flows
	std::vector< std::vector< LinkDesc > >::iterator link; // 2D iterator
	for ( link = link_.begin() ; link != link_.end() ; ++link ) { // go through every link w/ iterator
		for ( int s = 0 ; s < 4 ; s++ ) { // go through every service in each link
			CircularList<FlowDesc>& rr = link[0][s].rr_; // since we're incrementing link itself, always refer to link[0]

			bool changed = false;

			for ( unsigned int i = 0 ; i < rr.size() ; i++ ) {
				// get the current traffic flow
				FlowDesc& flow = rr.current();

				// remove the traffic flow if it has to be considered as inactive, i.e.
				// there are not any enqueued PDUs and the user-specified flow timeout
				// interval has expired since the last PDU has been received
				if ( NOW - flow.last_ > interval_ && flow.queue_.empty() ) {
					rr.erase ();
					changed = true;
				} else {
					rr.move ();
				}
			}
			// recompute weights, if needed
			if ( changed ) recompute (rr);
		}
	}
	// restart the timer
	timer_.start ( interval_ );
}
