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

#include <wimsh_bwmanager.h>

#include <wimsh_mac.h>
#include <wimsh_packet.h>

/*
 *
 * class WimshBwManager
 *
 */

WimshBwManager::WimshBwManager (WimshMac* m) : mac_ (m), timer_ (this)
{
	// resize the vectors to the time horizon (in frames)
	grants_.resize (HORIZON);
	dst_.resize (HORIZON);
	channel_.resize (HORIZON);
	service_.resize (HORIZON);
	src_.resize (HORIZON);
	uncoordsch_.resize (HORIZON);

	for ( unsigned int i = 0 ; i < HORIZON ; i++ ) {
		// clear all the bits of the frame maps
		grants_[i].reset();

		// resize the channel and destination vectors
		dst_[i].resize (MAX_SLOTS);
		channel_[i].resize (MAX_SLOTS);
		service_[i].resize (MAX_SLOTS);
		src_[i].resize (MAX_SLOTS);
		uncoordsch_[i].resize (MAX_SLOTS);

		// set all channels to 0
		for ( unsigned int j = 0 ; j < MAX_SLOTS ; j++ ) {
			channel_[i][j]		= 0;
			service_[i][j]		= 9;
			dst_[i][j]			= UINT_MAX;
			src_[i][j]			= UINT_MAX;
			uncoordsch_[i][j]	= UINT_MAX;
		}
	}

   // start the timer for the first time to expire at the beginning
   // of the first data subframe
   timer_.start (mac_->phyMib()->controlDuration());

	lastSlot_ = 0;
}

void
WimshBwManager::handle ()
{
	if ( WimaxDebug::trace("WBWM::handle") ) fprintf (stderr,
			"%.9f WBWM::handle     [%d] frame %d lastSlot_ %d\n",
			NOW, mac_->nodeId(), mac_->frame() % HORIZON, lastSlot_);

	const unsigned int F = mac_->frame() % HORIZON;        // alias
	const unsigned int N = mac_->phyMib()->slotPerFrame(); // alias

	bool status = true;       // tx = true, rx = false
	WimaxNodeId  dst = UINT_MAX;     // only meaningful with tx
	unsigned int channel = 0; // channel identifier
	unsigned int start;       // start minislot index of the next event
	unsigned int range = 0;   // minislot range of the next event
	unsigned int service = wimax::N_SERV_CALSS;		// traffic service class
	unsigned int undsch = UINT_MAX;

	// search for next frame to transmit requests (turn on bwmanager_frr flags)
	// these frames are assigned at end of request procedure in bwmanager_frr
	for ( unsigned int ngh = 0 ; ngh < mac_->nneighs() ; ngh++ ) {
		for ( unsigned int s = 0 ; s < wimax::N_SERV_CALSS ; s++ ) {
			if ( s == 2 ) continue;
			if ( mac_->frame() == nextFrame_[ngh][s] ) {
					startHorizon_[ngh][s] = true;
			}
		}

		if ( mac_->frame() == nextFrame_[ngh][2] && mac_->frame() > 0 && lastSlot_ == 0) {
			search_tx_slot (ngh, 0);
			startHorizon_[ngh][2] = true;
			unDschState_[ngh][F] = 0;
		}

		if ( mac_->frame() == rtpsDschFrame_[ngh] && mac_->frame() > 0 && lastSlot_ == 0) {
			unsigned int state = unDschState_[ngh][F];
			search_tx_slot (ngh, state);
		}
	}

	// the loop below starts from the next available slot from the last call
	// and goes on until either the data subframe ends, or there is a
	// change in the mode (tx/rx) or destination node (if tx) or channel

	for ( ; lastSlot_ < N ; lastSlot_++ ) {
		if ( range == 0 ) {
			status = grants_[F][lastSlot_];
			undsch = uncoordsch_[F][lastSlot_];
			if ( status == true ) {
				dst = dst_[F][lastSlot_];
				service = service_[F][lastSlot_]; }
			channel = channel_[F][lastSlot_];
			start = lastSlot_;
			range = 1;
			if ( WimaxDebug::debuglevel() > WimaxDebug::lvl.bwmgr_handle_ ) fprintf (stderr,
					"(BwMgr)!1 status %u dst %d service %d src %d channel %d undsch %d slot %d\n", status,
						dst_[F][lastSlot_], service_[F][lastSlot_], src_[F][lastSlot_], channel_[F][lastSlot_],
							undsch, lastSlot_ );

		} else {
				if ( WimaxDebug::debuglevel() > WimaxDebug::lvl.bwmgr_handle_ ) fprintf (stderr,
						"(BwMgr)!  status %u dst %d service %d src %d channel %d undsch %u slot %d\n", (bool)grants_[F][lastSlot_],
							dst_[F][lastSlot_], service_[F][lastSlot_], src_[F][lastSlot_], channel_[F][lastSlot_],
							(unsigned int)uncoordsch_[F][lastSlot_], lastSlot_ );

			if ( channel_[F][lastSlot_] == channel && uncoordsch_[F][lastSlot_] == undsch &&
				  grants_[F][lastSlot_] == status &&
				  ( ( status == true && dst_[F][lastSlot_] == dst &&
						service_[F][lastSlot_] == service ) ||
							( status == false ) )
				) ++range;
			else break;
		}
	}

	if ( undsch != UINT_MAX ) {
		// create MSH-DSCH message on coordinator module and transmite at mac module
		unsigned int ndx = mac_->neigh2ndx (undsch);
		bool grant = ( unDschState_[ndx][F] == 1 ) ? true : false;
		if ( WimaxDebug::debuglevel() > WimaxDebug::lvl.bwmgr_uncoordrange_ ) fprintf (stderr,
				"(UncrdRng)!!!uncoordinated MSH-DSCH message range %d dst %d gnt %d \n",range, undsch , grant );
		mac_->uncoordinated_opportunity (undsch, grant);
	} else if ( status == true && undsch == UINT_MAX ) {
		// set transmit mode on channel 0 towards dst
		mac_->transmit (range, dst, channel, service);
	} else {
		// set receive mode on channel 0
		mac_->receive (channel);
	}

	// if the whole frame has been considered, then the timer is set to
	// expire at the beginning of the next data subframe

	if ( lastSlot_ == N ) {
		timer_.start (
				  mac_->phyMib()->nextFrame()
				+ mac_->phyMib()->controlDuration()
				- NOW);

		// also, we invalidate the entries of the current frame
		invalidate (F);

		// reset the lastSlot_ variable to the first minislot
		lastSlot_ = 0;

	// otherwise, the timer is set to expire at the beginning of the
	// minislot after the current scheduled ones
	} else {
		timer_.start ( range * mac_->phyMib()->slotDuration() );
	}
}

void
WimshBwManager::invalidate (unsigned int F)
{
	for ( unsigned int i = 0 ; i < MAX_SLOTS ; i++ ) {
		if ( service_[F][i] == 3 ) { }
		else {
			channel_[F][i] = 0;
			grants_[F][i]  = 0;
			service_[F][i] = 9;
			dst_[F][i]     = UINT_MAX;
			src_[F][i]     = UINT_MAX;
		}
		uncoordsch_[F][i] = UINT_MAX;
	}
}

/*
 *
 * class WimshBwManagerDummy
 *
 */

WimshBwManagerDummy::WimshBwManagerDummy (WimshMac* m) :
	WimshBwManager (m)
{
	// nihil
}

int
WimshBwManagerDummy::command (int argc, const char*const* argv)
{
	if ( argc == 4 && strcmp (argv[0], "static") == 0 ) {
      // set the destination node, start and range values, respectively
      setRange ( atoi(argv[1]), atoi(argv[2]), atoi(argv[3]) );
      return TCL_OK;
	}

	return TCL_ERROR;
}

void
WimshBwManagerDummy::recvMshDsch (WimshMshDsch* dsch)
{
	if ( WimaxDebug::trace("WBWM::recvMshDsch") ) fprintf (stderr,
			"%.9f WBWM::recvMshDsch[%d]\n", NOW, mac_->nodeId());

	// for each grant to this node, add the range to the grants data structure
	std::list<WimshMshDsch::GntIE>& gnt = dsch->gnt();

	std::list<WimshMshDsch::GntIE>::iterator it;
	for ( it = gnt.begin() ; it != gnt.end() ; ++it ) {

		// we only consider the grants from the granter to the requester
		// (ie. grants, not the confirmations) that are addressed to this node
		if ( it->fromRequester_ == false && it->nodeId_ == mac_->nodeId() ) {
			// we assume that it->channel_ is 0
			// we assume that the persistence is not 'forever'
			// we ignore cancellations (ie. persistence = 'cancel')

			// for each frame in the persistence
			for ( unsigned int f = 0 ;
					f < WimshMshDsch::pers2frames (it->persistence_) ; f++ ) {
				unsigned int F = ( it->frame_ + f ) % HORIZON;

				// for each minislot into the range
				for ( unsigned int s = 0 ; s < it->range_ ; s++ ) {
					unsigned int S = s + it->start_;

					grants_[F][S] = true;
					dst_[F][S] = dsch->src();
				}  // for each minislot into the range
			} // for each frame in the persistence
		}
	} // for each grant in the MSH-DSCH message
}

void
WimshBwManagerDummy::schedule (WimshMshDsch* dsch, unsigned int dst)
{
	if ( WimaxDebug::trace("WBWM::schedule") ) fprintf (stderr,
			"%.9f WBWM::schedule   [%d]\n", NOW, mac_->nodeId());

	// grant bandwidth according to the static table
	std::vector<AllocationDesc>::iterator it;
	for ( it = alloc_.begin() ; it != alloc_.end() ; ++it ) {
		WimshMshDsch::GntIE gnt;
		gnt.nodeId_ = it->node_;
		gnt.frame_ =
			  mac_->frame()
			+ (unsigned int) ceil (
				(fabs ( mac_->h (it->node_) - mac_->phyMib()->controlDuration() ))
				  / mac_->phyMib()->frameDuration()
			  );  // :TODO: this expression should be verified
		gnt.start_ = it->start_;
		gnt.range_ = it->range_;
		gnt.fromRequester_ = false;
		gnt.persistence_ = WimshMshDsch::FRAME1;
		gnt.channel_ = 0;

		dsch->add (gnt);
	}
}

