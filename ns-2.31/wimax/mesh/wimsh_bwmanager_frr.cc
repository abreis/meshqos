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

#include <wimsh_bwmanager_frr.h>

#include <wimsh_mac.h>
#include <wimsh_topology.h>
#include <wimsh_packet.h>
#include <wimsh_scheduler.h>

#include <math.h>
#include <random.h>
#include <stat.h>

WimshBwManagerFairRR::WimshBwManagerFairRR (WimshMac* m) :
	WimshBwManager (m), wm_ (m)
{
	// resize the unavailabilities to receive of this node, confirmed or not
	busy_.resize (HORIZON);
	busy_UGS_.resize (HORIZON);
	busy_NRTPS_.resize (HORIZON);
	unconfirmedSlots_.resize (HORIZON);
	unconfirmedSlots_UGS_.resize (HORIZON);
	unconfirmedSlots_NRTPS_.resize (HORIZON);

	for ( unsigned int i = 0 ; i < HORIZON ; i++ ) {
		// clear all the bits of the busy_ and unconfirmedSlots_ bitmap
		busy_[i].reset();
		busy_UGS_[i].reset();
		unconfirmedSlots_[i].reset();
		unconfirmedSlots_UGS_[i].reset();
	}

	regrantOffset_         = 1;
	regrantDuration_       = 1;
	avlAdvertise_          = true;
	regrantEnabled_        = true;
	fairGrant_             = true;
	fairRequest_           = true;
	fairRegrant_           = true;
	deficitOverflow_       = false;
	grantFitRandomChannel_ = false;
	sameRegrantHorizon_    = false;
	maxDeficit_            = 0;
	maxBacklog_            = 0;
	roundDuration_         = 0;
	ddTimeout_             = 0;
	ddTimer_               = 0;
	minGrant_              = 1;
	nrtpsMinSlots_		   = 0;
}

int
WimshBwManagerFairRR::command (int argc, const char*const* argv)
{
	if ( argc == 2 && strcmp (argv[0], "availabilities") == 0 ) {
		if ( strcmp (argv[1], "on") == 0 ) {
			avlAdvertise_ = true;
		} else if ( strcmp (argv[1], "off") == 0 ) {
			avlAdvertise_ = false;
		} else {
			fprintf (stderr, "invalid availabilities '%s' command. "
					"Choose either 'on' or 'off'", argv[1]);
			return TCL_ERROR;
		}
      return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "regrant") == 0 ) {
		if ( strcmp (argv[1], "on") == 0 ) {
			regrantEnabled_ = true;
		} else if ( strcmp (argv[1], "off") == 0 ) {
			regrantEnabled_ = false;
		} else {
			fprintf (stderr, "invalid regrant '%s' command. "
					"Choose either 'on' or 'off'", argv[1]);
			return TCL_ERROR;
		}
      return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "regrant-same-horizon") == 0 ) {
		if ( strcmp (argv[1], "on") == 0 ) {
			sameRegrantHorizon_ = true;
		} else if ( strcmp (argv[1], "off") == 0 ) {
			sameRegrantHorizon_ = false;
		} else {
			fprintf (stderr, "invalid regrant-same-horizon '%s' command. "
					"Choose either 'on' or 'off'", argv[1]);
			return TCL_ERROR;
		}
      return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "regrant-offset") == 0 ) {
		regrantOffset_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "regrant-duration") == 0 ) {
		regrantDuration_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "dd-timeout") == 0 ) {
		if ( atoi (argv[1]) < 0 ) {
			fprintf (stderr, "Invalid deadlock detection timeout '%d'. "
					"Choose a number greater than or equal to zero\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		ddTimeout_ = atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "round-duration") == 0 ) {
		if ( atoi (argv[1]) <= 0 ) {
			fprintf (stderr, "Invalid round duration '%d'. "
					"Choose a number greater than zero (in bytes)\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		roundDuration_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "max-deficit") == 0 ) {
		if ( atoi (argv[1]) < 0 ) {
			fprintf (stderr, "Invalid maximum deficit amount '%d'. "
					"Choose a number greater than or equal to zero (in bytes)\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		maxDeficit_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "min-grant") == 0 ) {
		if ( atoi (argv[1]) < 1 ) {
			fprintf (stderr, "Invalid minimum grant size '%d'. Choose "
					"a number greater than or equal to one (in OFDM symbols)\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		minGrant_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "nrtps-min-slots") == 0 ) {
		if ( atoi (argv[1]) < 1 ) {
			fprintf (stderr, "Invalid minimum number of nrtPS slots '%d'. "
					"Choose a number greater than zero\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		nrtpsMinSlots_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "max-backlog") == 0 ) {
		if ( atoi (argv[1]) < 0 ) {
			fprintf (stderr, "Invalid maximum backlog amount '%d'. "
					"Choose a number greater than or equal to zero (in bytes)\n",
					atoi (argv[1]));
			return TCL_ERROR;
		}
		maxBacklog_ = (unsigned int) atoi (argv[1]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[0], "fairness") == 0 ) {
		if ( strcmp (argv[1], "grant") == 0 ) {
			fairGrant_ = true;
		} else if ( strcmp (argv[1], "regrant") == 0 ) {
			fairRegrant_ = true;
		} else if ( strcmp (argv[1], "request") == 0 ) {
			fairRequest_ = true;
		} else if ( strcmp (argv[1], "no") == 0 ) {
			fairRequest_ = false;
			fairRegrant_ = false;
			fairGrant_ = false;
		} else {
			fprintf (stderr, "unknown fairness specifier '%s'. "
					"Choose 'grant', 'regrant' or 'no'\n", argv[1]);
			return TCL_ERROR;
		}
		return TCL_OK;
	} else if ( argc == 3 && strcmp (argv[0], "grant-fit") == 0 ) {
		if ( strcmp (argv[1], "channel" ) == 0 ) {
			if ( strcmp (argv[2], "random") == 0 ) {
				grantFitRandomChannel_ = true;
			} else if ( strcmp (argv[2], "first") == 0 ) {
				grantFitRandomChannel_ = false;
			}
		} else {
			fprintf (stderr, "unknown grant-fit specifier '%s %s'\n",
					argv[1], argv[2]);
			return TCL_ERROR;
		}
		return TCL_OK;
	} else if ( strcmp (argv[0], "wm") == 0 ) {
		return wm_.command (argc - 1, argv + 1);
	}

	return TCL_ERROR;
}

void
WimshBwManagerFairRR::initialize ()
{
	const unsigned int neighbors = mac_->nneighs();

	// resize and clear the bw request/grant data structure for each service class
	neigh_.resize (neighbors);
	startHorizon_.resize (neighbors);
	nextFrame_.resize (neighbors);
	send_rtps_together_.resize (neighbors);
	for ( unsigned int ngh = 0 ; ngh < neighbors ; ngh++ ) {
		neigh_[ngh].resize (wimax::N_SERV_CLASS);
		startHorizon_[ngh].resize (wimax::N_SERV_CLASS);
		nextFrame_[ngh].resize (wimax::N_SERV_CLASS);
		send_rtps_together_[ngh] = false;
		for ( unsigned int i = 0 ; i < wimax::N_SERV_CLASS ; i++ ) {
			startHorizon_[ngh][i] = true;
			nextFrame_[ngh][i] = 0;
		}
	}

	// resize and clear the neighbors' unavailabilites bitmaps
	neigh_tx_unavl_.resize (neighbors);
	neigh_tx_unavl_UGS_.resize (neighbors);
	neigh_tx_unavl_NRTPS_.resize (neighbors);
	for ( unsigned int ngh = 0 ; ngh < neighbors ; ngh++ ) {
		neigh_tx_unavl_[ngh].resize (mac_->nchannels());
		neigh_tx_unavl_UGS_[ngh].resize (mac_->nchannels());
		neigh_tx_unavl_NRTPS_[ngh].resize (mac_->nchannels());
		for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
			neigh_tx_unavl_[ngh][ch].resize (HORIZON);
			neigh_tx_unavl_UGS_[ngh][ch].resize (HORIZON);
			neigh_tx_unavl_NRTPS_[ngh][ch].resize (HORIZON);
			for ( unsigned int f = 0 ; f < HORIZON ; f++ ) {
				neigh_tx_unavl_[ngh][ch][f].reset();
				neigh_tx_unavl_UGS_[ngh][ch][f].reset();
				neigh_tx_unavl_NRTPS_[ngh][ch][f].reset();
			}
		}
	}

	self_rx_unavl_.resize (mac_->nchannels());
	self_tx_unavl_.resize (mac_->nchannels());
	self_rx_unavl_UGS_.resize (mac_->nchannels());
	self_tx_unavl_UGS_.resize (mac_->nchannels());
	self_rx_unavl_NRTPS_.resize (mac_->nchannels());
	self_tx_unavl_NRTPS_.resize (mac_->nchannels());
	for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
		self_rx_unavl_[ch].resize (HORIZON);
		self_tx_unavl_[ch].resize (HORIZON);
		self_rx_unavl_UGS_[ch].resize (HORIZON);
		self_tx_unavl_UGS_[ch].resize (HORIZON);
		self_rx_unavl_NRTPS_[ch].resize (HORIZON);
		self_tx_unavl_NRTPS_[ch].resize (HORIZON);
		for ( unsigned int f = 0 ; f < HORIZON ; f++ ) {
			self_rx_unavl_[ch][f].reset();
			self_tx_unavl_[ch][f].reset();
			self_rx_unavl_UGS_[ch][f].reset();
			self_tx_unavl_UGS_[ch][f].reset();
			self_rx_unavl_NRTPS_[ch][f].reset();
			self_tx_unavl_NRTPS_[ch][f].reset();
		}
	}

	unDschState_.resize (neighbors);
	rtpsDschFrame_.resize (neighbors);
	for ( unsigned int i = 0 ; i < neighbors ; i++ ) {
		unDschState_[i].resize (HORIZON);
		rtpsDschFrame_[i] = 0;
		for ( unsigned int f = 0 ; f < HORIZON ; f++ )
			unDschState_[i][f] = 0;
	}

	unconfirmed_[0].resize (neighbors);
	unconfirmed_[1].resize (neighbors);

	// initialize the weight manager
	wm_.initialize ();
}

void
WimshBwManagerFairRR::recvMshDsch (WimshMshDsch* dsch)
{
	if ( WimaxDebug::trace("WBWM::recvMshDsch") ) fprintf (stderr,
			"%.9f WBWM::recvMshDsch[%d]\n", NOW, mac_->nodeId());

	// breakpoint triggers
	float tnow = NOW;
	unsigned int tnode = mac_->nodeId();



	rcvAvailabilities(dsch);	// we interpret AvlIEs first so we can correctly grant bandwidth afterwards
	rcvGrants(dsch);			// we interpret GntIEs second so that bandwidth cancelations are processed before requests
	rcvRequests(dsch);
}

void
WimshBwManagerFairRR::rcvGrants (WimshMshDsch* dsch)
{
	// breakpoint triggers
	unsigned int tnode = mac_->nodeId();

	// true if we're interpreting an uncoordinated DSCH's grants
	bool uncrdDSCH = dsch->reserved();
	// ?
	bool rcvCnfs = false;

	// get the local identifier of the node who sent this MSH-DSCH
	unsigned int ndx = mac_->neigh2ndx (dsch->src());

	// in any case we set the rcvCnf_ flag to true, so that the grant()
	// function will grant any unconfirmed bandwidth with a higher
	// priority than that of "fresh" bandwidth grants
	// this flag is only set for rtPS if the DSCH is uncoordinated
	if ( uncrdDSCH )
		neigh_[ndx][wimax::RTPS].rcvCnf_ = true;
	else
		for ( unsigned int j = 0 ; j < wimax::N_SERV_CLASS ; j++) {
			if ( j == wimax::RTPS ) continue;
			neigh_[ndx][j].rcvCnf_ = true;
		}

	// get the list of grants/confirmations
	std::list<WimshMshDsch::GntIE>& gnt = dsch->gnt();
	std::list<WimshMshDsch::GntIE>::iterator it;

	// the following cycle goes through every GrantIE in the received DSCH
	for ( it = gnt.begin() ; it != gnt.end() ; ++it ) {
		// grant service class
		unsigned int serv = it->service_;

		/*
		 * cancel
		 */

		if ( it->persistence_ == WimshMshDsch::CANCEL ) {

			// if this cancelation is not addressed to this node, ignore it
			if ( it->nodeId_ != mac_->nodeId() ) continue;

			if ( WimaxDebug::trace("WBWM::rcvGntIE") ) fprintf (stderr,
					"%.9f WBWM::rcvGntIE   [%d] cancelIE frame %d start %d range %d serv %d\n",
					NOW, mac_->nodeId(), it->frame_, it->start_, it->range_, it->service_);

			// for UGS, we cancel the reservations; for other services, we note the cancel request
			if ( it->service_ == wimax::UGS ) {
				// cancel reservation
				setSlots (busy_UGS_, 						it->frame_, 128, it->start_, it->range_, false);
				setSlots (self_rx_unavl_UGS_[it->channel_],	it->frame_, 128, it->start_, it->range_, false);
//				setSlots (neigh_tx_unavl_UGS_[sndx][ch],	it->frame_, 128, it->start_, it->range_, false);

				setSlots (dst_, 	it->frame_, 128, it->start_, it->range_, UINT_MAX);
				setSlots (grants_, 	it->frame_, 128, it->start_, it->range_, false);
				setSlots (service_, it->frame_, 128, it->start_, it->range_, 9);
			} else {
				// store the bandwidth cancel order for the service
			}

			continue;
		}

		//
		// grant
		//

		// if this grant is addressed to us, we first schedule a confirmation
		// message to be advertised as soon as possible by this node,
		// then mark the slots as unconfirmed unavailable
		// moreover, we update the amount of granted bandwidth
		if ( it->fromRequester_ == false && it->nodeId_ == mac_->nodeId() ) {

			// schedule the confirmation to be sent asap
			// modify the destination NodeID and the grant direction
			it->nodeId_ = dsch->src();
			it->fromRequester_ = true; // indicates a grant confirmation

			/*
			 * add the IE to the list of unconfirmed grants
			 * there is a separation in the list unconfirmed_:
			 * - unconfirmedÉ[1][nodeid] -> uncoordinated DSCH grants
			 * - unconfirmed_[0][0] -> coordinated DSCH grants
			 */
			if ( uncrdDSCH )
				unconfirmed_[1][ndx].push_back (*it);
			else
				unconfirmed_[0][0].push_back (*it);

			// number of frames over which the grant spans
			// we assume that bandwidth is never granted in the past
			unsigned int frange = WimshMshDsch::pers2frames(it->persistence_);

			if ( serv == wimax::UGS ) {
				// if we received a grant for UGS, mark the granted slots in unconfirmedSlots_UGS_
				setSlots (unconfirmedSlots_UGS_, it->frame_, frange,
						it->start_, it->range_, true);
			} else { // not UGS
				if ( serv == wimax::NRTPS ) {
					// NOTE: '?:' operation below looks reversed (should force the MinSlots if the range is smaller, not larger)
					unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;
					//unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? it->range_ : nrtpsMinSlots_;

					// mark the granted slot range for nrtPS in unconfirmedSlots_NRTPS_
					setSlots (unconfirmedSlots_NRTPS_, it->frame_, frange,
							it->start_, slots, true);

					if ( WimaxDebug::trace("WBWM::rcvGrants") ) fprintf (stderr,
								"%.9f WSCH::rcvGrants  [%d] mark w/ nrtpsMin_ frame %d start %d slots %d\n",
								NOW, mac_->nodeId(), it->frame_, it->start_, slots);
				}
				// mark the granted slot range in unconfirmed_ ; NOTE: does not run for UGS, why?
				setSlots (unconfirmedSlots_, it->frame_, frange,
						it->start_, it->range_, true);
			}

			// update the amount of bytes granted from this node
			neigh_[ndx][serv].gnt_out_ +=
				frange * mac_->slots2bytes (ndx, it->range_, true);
			Stat::put ( "wimsh_gnt_out", mac_->index(),
				frange * mac_->slots2bytes (ndx, it->range_, true) );

			// we enforce the number of granted bytes to be smaller than
			// that of requested bytes
			neigh_[ndx][serv].gnt_out_ =
				( neigh_[ndx][serv].gnt_out_ < neigh_[ndx][serv].req_out_ ) ?
				neigh_[ndx][serv].gnt_out_ : neigh_[ndx][serv].req_out_;      // XXX

			// if we have a grant for an rtPS service, schedule an
			// uncoordinated MSH-DSCH confirmation response
			// mac_->bwmanager()->searchTXslot (ndx, false);
			if ( serv == wimax::RTPS ) { // NOTE: see this
				rtpsDschFrame_[ndx] = mac_->frame() + 1;
				unDschState_[ndx][(mac_->frame() + 1) % HORIZON] = 2;

				if ( WimaxDebug::trace("WBWM::rcvGrants") ) {
					fprintf (stderr,
							"%.9f WBWM::rcvGrants  [%d] Scheduled a grant-confirm to ndx %d frame %d\n",
							NOW, mac_->nodeId(), ndx, rtpsDschFrame_[ndx]);
				}

			}
		} // if ( it->fromRequester_ == false && it->nodeId_ == mac_->nodeId() )

		// if this grant is not addressed to us, then add a pending availability
		// and update the bandwidth grant/confirm data structures so that:
		// - we will not grant bandwidth to the requester on any channel
		// - we will not grant bandwidth to the granter's neighbors on the
		//   same channel of this grant
		// - we will not confirm bandwidth on the same channel of this grant
		if ( it->fromRequester_ == false && it->nodeId_ != mac_->nodeId() ) {

			//
			// create a new availability and add it to the pending list
			//

			WimshMshDsch::AvlIE avl;
			avl.frame_ = it->frame_;
			avl.start_ = it->start_;
			avl.range_ = it->range_;
			avl.direction_ = WimshMshDsch::UNAVAILABLE;
			avl.persistence_ = it->persistence_;
			avl.channel_ = it->channel_;
			avl.service_ = it->service_;

			// push the new availability into the pending list
			if ( uncrdDSCH )
				availabilities_[1].push_back (avl);
			else
				availabilities_[0].push_back (avl);

			//
			// the granter is unable to receive data from this node while
			// it is receiving data from any of its neighbors.
			// Thus, we mark the granted slots as unavailable on all channels
			//

			// number of frames over which the grant spans
			// we assume that bandwidth is never granted in the past
			unsigned int frange = WimshMshDsch::pers2frames(it->persistence_);

			// set the minislots as unavailable to transmit to granter
			for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
				if ( serv == wimax::UGS ) {
					setSlots (neigh_tx_unavl_UGS_[ndx][ch], it->frame_, frange,
							it->start_, it->range_, true);
				} else {
					if ( serv == wimax::NRTPS ) {
						unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

						setSlots (neigh_tx_unavl_NRTPS_[ndx][ch], it->frame_, frange,
								it->start_, slots, true);
//						if ( WimaxDebug::enabled() ) fprintf (stderr,
//							"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//								it->frame_, it->start_, slots);
					}

					setSlots (neigh_tx_unavl_[ndx][ch], it->frame_, frange,
							it->start_, it->range_, true);
				}
				setSlots (service_, it->frame_, frange,	it->start_, it->range_, serv);
			}

			//
			// if the requester is one of our neighbors, then we will not
			// be able to grant bandwidth to it (if requested) into the
			// set of granted slots on any channel
			//

			if ( mac_->topology()->neighbors (it->nodeId_, mac_->nodeId()) ) {
				// index of the requester
				const unsigned int ndx = mac_->neigh2ndx (it->nodeId_);

				// set the minislots as unavailable to transmit to requester
				for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
					if ( serv == wimax::UGS ) {
						setSlots (neigh_tx_unavl_UGS_[ndx][ch], it->frame_, frange,
								it->start_, it->range_, true);
					} else {
						if ( serv == wimax::NRTPS ) {
							unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

							setSlots (neigh_tx_unavl_NRTPS_[ndx][ch], it->frame_, frange,
									it->start_, slots, true);
//							if ( WimaxDebug::enabled() ) fprintf (stderr,
//								"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//									it->frame_, it->start_, slots);
						}

						setSlots (neigh_tx_unavl_[ndx][ch], it->frame_, frange,
								it->start_, it->range_, true);
					}
					setSlots (service_, it->frame_, frange,	it->start_, it->range_, serv);
				}
			}

			//
			// all neighbors of the granter will not be able to transmit
			// in the set of granted slots on the specified channel, which
			// is thus set as unavailable for transmission for all the
			// neighbors of the granter which are also our neighbors
			// ???? what's the difference for this condition to taht one below
			//

			std::vector<WimaxNodeId> gntNeigh;  // array of the granter's neighbors
			mac_->topology()->neighbors (dsch->src(), gntNeigh); // retrieve them
			for ( unsigned int ngh = 0 ; ngh < gntNeigh.size() ; ngh++ ) {

				// skip the requester, which has been already managed,
				// and nodes which are not our neighbors
				if ( gntNeigh[ngh] == it->nodeId_ ||
					  ! mac_->topology()->neighbors (gntNeigh[ngh], mac_->nodeId()) )
					continue;

				// otherwise, set the granted slots as unavailable
				const unsigned int ndx = mac_->neigh2ndx (gntNeigh[ngh]); // index

				if ( serv == wimax::UGS ) {
					setSlots (neigh_tx_unavl_UGS_[ndx][it->channel_],
							it->frame_, frange, it->start_, it->range_, true);
				} else {
					if ( serv == wimax::NRTPS ) {
						unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

						setSlots (neigh_tx_unavl_NRTPS_[ndx][it->channel_], it->frame_, frange,
								it->start_, slots, true);
//						if ( WimaxDebug::enabled() ) fprintf (stderr,
//							"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//								it->frame_, it->start_, slots);
					}

					setSlots (neigh_tx_unavl_[ndx][it->channel_],
							it->frame_, frange, it->start_, it->range_, true);
				}
				setSlots (service_, it->frame_, frange, it->start_,
						it->range_, serv);
			}

			//
			// we are not able to transmit in the granted slots on the specified
			// channel (ie. to confirm bandwidth, even though it has been granted)
			//
			if ( serv == wimax::UGS ) {
				setSlots (self_tx_unavl_UGS_[it->channel_],
						it->frame_, frange, it->start_, it->range_, true);
			} else {
				if ( serv == wimax::NRTPS ) {
					unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

					setSlots (self_tx_unavl_NRTPS_[it->channel_], it->frame_, frange,
							it->start_, slots, true);
//					if ( WimaxDebug::enabled() ) fprintf (stderr,
//						"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//							it->frame_, it->start_, slots);
				}

				setSlots (self_tx_unavl_[it->channel_],
						it->frame_, frange, it->start_, it->range_, true);
			}
			setSlots (service_, it->frame_, frange, it->start_,
					it->range_, serv);
		} // if ( it->fromRequester_ == false && it->nodeId_ != mac_->nodeId() )

		//
		// confirmation
		//

		// if the confirmation is addressed to a node which is not in
		// our first-hop neighborhood (nor to this node itself), then
		// that node cannot transmit in the confirmed minislots on all channels
		// and we cannot receive in the confirmed minislots on the
		// specified channel
		if ( it->fromRequester_ == true &&
				! mac_->topology()->neighbors (it->nodeId_, mac_->nodeId()) ) {

			// receive confirmations flag
			rcvCnfs = true;

			if ( it->frame_ == 0 && it->start_ == 0 && it->range_ == 0 ) {
				if ( WimaxDebug::enabled() ) fprintf (stderr,"Null confirmation detected, aborting.\n");
				abort();
			}

			// convert the <frame, persistence> pair to the actual <frame, range>
			unsigned int fstart;   // start frame number
			unsigned int frange;   // frame range
			realPersistence (it->frame_, it->persistence_, fstart, frange);

			// set the minislots as unavailable for reception on all channels
			for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
				if ( serv == wimax::UGS ) {
					setSlots (neigh_tx_unavl_UGS_[ndx][ch], fstart, frange,
							it->start_, it->range_, true);
				} else {
					if ( serv == wimax::NRTPS ) {
						unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

						setSlots (neigh_tx_unavl_NRTPS_[ndx][ch], fstart, frange,
								it->start_, slots, true);
//						if ( WimaxDebug::enabled() ) fprintf (stderr,
//							"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//								it->frame_, it->start_, slots);
					}

					setSlots (neigh_tx_unavl_[ndx][ch], fstart, frange,
						it->start_, it->range_, true);
				}
			}

			// set the minislots as unavailable for reception at this node
			if ( serv == wimax::UGS ) {
				setSlots (self_rx_unavl_UGS_[it->channel_], fstart, frange,
						it->start_, it->range_, true);
			} else {
				if ( serv == wimax::NRTPS ) {
					unsigned int slots = ( it->range_ > nrtpsMinSlots_ ) ? nrtpsMinSlots_ : it->range_;

					setSlots (self_rx_unavl_NRTPS_[it->channel_], fstart, frange,
							it->start_, slots, true);
//					if ( WimaxDebug::enabled() ) fprintf (stderr,
//						"!!marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//							it->frame_, it->start_, slots);
				}

				setSlots (self_rx_unavl_[it->channel_], fstart, frange,
					it->start_, it->range_, true);
			}
			// TODO
//			setSlots (service_, fstart, frange, it->start_,
//					it->range_, serv);
		}

		// if the confirmation is addressed to this node, then update
		// the counter of the incoming confirmed bandwidth
		// and set the (only) radio to listen to the confirmed channel
		if ( it->fromRequester_ == true && it->nodeId_ == mac_->nodeId() ) {

			// receive confirmations flag
			rcvCnfs = true;

			if ( it->frame_ == 0 && it->start_ == 0 && it->range_ == 0 ) {
				if ( WimaxDebug::enabled() ) fprintf (stderr,"0detectei uma confirmacao branca\n");
				abort();
			}

			// get the local identifier of the node who sent this MSH-DSCH
			unsigned int ndx = mac_->neigh2ndx (dsch->src());

			// get number of frames over which the confirmation spans
			// we assume that the persistence_ is not 'forever'
			// we assume that bandwidth requests are not canceled
			unsigned int frange = WimshMshDsch::pers2frames(it->persistence_);

			// listen to the specified channel in the confirmed set of slots
			setSlots (channel_, it->frame_, frange,
					it->start_, it->range_, it->channel_);

			setSlots (src_, it->frame_, frange, it->start_, it->range_, dsch->src());

			// update the number of bytes confirmed
			neigh_[ndx][serv].cnf_in_ +=
				frange * mac_->slots2bytes (ndx, it->range_, true);
			Stat::put ( "wimsh_cnf_in", mac_->index(),
				frange * mac_->slots2bytes (ndx, it->range_, true) );

//				if ( WimaxDebug::enabled() ) fprintf (stderr,"subi o lastCnf_ ndx %d s %d\n", ndx, serv);

			//if ( uncrdDSCH && rcvCnfs && neigh_[ndx][serv].cnf_in_ < neigh_[ndx][serv].gnt_in_ ) {
			//if ( s == wimax::RTPS ) {
				// schedule uncoordinated MSH-DSCH to regrant left bytes
			//	if ( WimaxDebug::enabled() ) fprintf (stderr,"rcvGrants enviei uma msg REgrant do servico 2 nodeId %d ndx %d\n",
			//				mac_->nodeId(),ndx);
				//mac_->bwmanager()->searchTXslot (ndx, false);
			//	rtpsDschFrame_[ndx] = mac_->frame() + 1;
			//}
		}
	} // process the next GrantIE in the DSCH
}

void
WimshBwManagerFairRR::rcvAvailabilities (WimshMshDsch* dsch)
{
	// breakpoint triggers
	unsigned int tnode = mac_->nodeId();

	// get the list of availabilities
	std::list<WimshMshDsch::AvlIE>& avl = dsch->avl();

	// get the index of the neighbor that sent this MSH-DSCH message
	unsigned int ndx = mac_->neigh2ndx (dsch->src());
	unsigned int nrtPS_slots [mac_->nneighs()];
	for (unsigned int i = 0; i < mac_->nneighs(); i++)
		nrtPS_slots[i] = nrtpsMinSlots_;

	std::list<WimshMshDsch::AvlIE>::iterator it;
	for ( it = avl.begin() ; it != avl.end() ; ++it ) {

		// convert the <frame, persistence> pair to the actual <frame, range>
		unsigned int fstart;   // start frame number
		unsigned int frange;   // frame range
		realPersistence (it->frame_, it->persistence_, fstart, frange);

		if ( it->service_ == wimax::UGS ) {

			// (cancel UGS service) applies to neigbours of the transmiter
			if ( it->direction_ == WimshMshDsch::RX_AVL &&
				mac_->topology()->neighbors (dsch->src(), mac_->nodeId()) ) {

				setSlots (self_rx_unavl_UGS_[it->channel_],
						  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
					setSlots (neigh_tx_unavl_UGS_[ndx][ch],
							  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

			} // (cancel UGS service) applies to neighbours of the receiver
			else if ( it->direction_ == WimshMshDsch::TX_AVL &&
					   mac_->topology()->neighbors (dsch->src(), mac_->nodeId()) ) {

				setSlots (self_tx_unavl_UGS_[it->channel_],
						  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
					setSlots (neigh_tx_unavl_UGS_[ndx][ch],
							  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

				std::vector<WimaxNodeId> gntNeigh;  // array of the granter's neighbors
				mac_->topology()->neighbors (dsch->src(), gntNeigh); // retrieve them
				for ( unsigned int ngh = 0 ; ngh < gntNeigh.size() ; ngh++ ) {

					// skip the requester, which has been already managed,
					// and nodes which are not our neighbors
					if ( ! mac_->topology()->neighbors (gntNeigh[ngh], mac_->nodeId()) )
						continue;

					// otherwise, set the granted slots as unavailable
					const unsigned int n = mac_->neigh2ndx (gntNeigh[ngh]); // index

					setSlots (neigh_tx_unavl_UGS_[n][it->channel_],
							  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

				// turn unavailable a range of slots (standard case)
			} else {
				setSlots (neigh_tx_unavl_UGS_[ndx][it->channel_],
						  fstart, frange, it->start_, it->range_, true);
				setSlots (service_, fstart, frange,
						  it->start_, it->range_, it->service_);
			}

		// for other services
		} else {

			// applies to neigbours of the transmiter
			if ( it->direction_ == WimshMshDsch::RX_AVL &&
					mac_->topology()->neighbors (dsch->src(), mac_->nodeId()) ) {

				setSlots (self_rx_unavl_[it->channel_],
						it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				setSlots (self_rx_unavl_UGS_[it->channel_],
						  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				setSlots (grants_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, false);

				setSlots (dst_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, 999);

				setSlots (src_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, 999);

				for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
					setSlots (neigh_tx_unavl_[ndx][ch],
							it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

			// applies to neighbours of the receiver
			} else if ( it->direction_ == WimshMshDsch::TX_AVL &&
					mac_->topology()->neighbors (dsch->src(), mac_->nodeId()) ) {

				setSlots (self_tx_unavl_[it->channel_],
						it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				setSlots (self_tx_unavl_UGS_[it->channel_],
						  it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);

				setSlots (busy_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
									it->start_, it->range_, false);

				setSlots (busy_UGS_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						  it->start_, it->range_, false);

				setSlots (unconfirmedSlots_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
									it->start_, it->range_, true);

				setSlots (unconfirmedSlots_UGS_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						  it->start_, it->range_, true);

				setSlots (service_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, it->service_);

				setSlots (grants_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, false);

				setSlots (dst_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, 999);

				setSlots (src_, it->frame_, WimshMshDsch::pers2frames(it->persistence_),
						it->start_, it->range_, 999);

				for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
					setSlots (neigh_tx_unavl_[ndx][ch],
							it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

				std::vector<WimaxNodeId> gntNeigh;  // array of the granter's neighbors
				mac_->topology()->neighbors (dsch->src(), gntNeigh); // retrieve them
				for ( unsigned int ngh = 0 ; ngh < gntNeigh.size() ; ngh++ ) {

					// skip the requester, which has been already managed,
					// and nodes which are not our neighbors
					if ( ! mac_->topology()->neighbors (gntNeigh[ngh], mac_->nodeId()) )
						continue;

					// otherwise, set the granted slots as unavailable
					const unsigned int n = mac_->neigh2ndx (gntNeigh[ngh]); // index

					setSlots (neigh_tx_unavl_[n][it->channel_],
							it->frame_, WimshMshDsch::pers2frames(it->persistence_), it->start_, it->range_, false);
				}

			// turn unavailable a range of slots (standard case)
			} else {
				// TODO
//				setSlots (neigh_tx_unavl_[ndx][it->channel_],
//						fstart, frange, it->start_, it->range_, true);
//
//				setSlots (neigh_tx_unavl_UGS_[ndx][it->channel_],
//						  fstart, frange, it->start_, it->range_, true);
//
//				setSlots (service_, fstart, frange,
//						it->start_, it->range_, it->service_);

				if ( it->service_ == wimax::NRTPS && nrtPS_slots[ndx] > 0 ) {

					unsigned int slots = ( it->range_ > nrtPS_slots[ndx] ) ? nrtPS_slots[ndx] : it->range_;

					setSlots (neigh_tx_unavl_NRTPS_[ndx][it->channel_], fstart, frange,
							it->start_, slots, true);

					nrtPS_slots[ndx] = nrtPS_slots[ndx] - slots;

//					if ( WimaxDebug::enabled() ) fprintf (stderr,
//						"!!avl marquei com nrtpsMin_ a frame %d start %d slots %d\n",
//							it->frame_, it->start_, slots);
				}
			}
		}
	}
}

void
WimshBwManagerFairRR::rcvRequests (WimshMshDsch* dsch)
{
	// get the list of bandwidth requests
	std::list<WimshMshDsch::ReqIE>& req = dsch->req();

	// get the index of the neighbor who sent this MSH-DSCH message
	unsigned int ndx = mac_->neigh2ndx (dsch->src());

	std::list<WimshMshDsch::ReqIE>::iterator it;
	for ( it = req.begin() ; it != req.end() ; ++it ) {

		// if this request is not addressed to this node, ignore it
		if ( it->nodeId_ != mac_->nodeId() ) continue;

		// get service class of request
		unsigned char s = it->service_;

		// cancel reservations
		if ( it->persistence_ == WimshMshDsch::CANCEL ) { // TODO: bw cancellation here
			cancel_Granter  (ndx, s);
			continue;
		}

		// indicate that there may be a new flow to the weight manager
		//wm_.flow (ndx, wimax::IN);

		// number of bytes requested
		unsigned requested =
			WimshMshDsch::pers2frames(it->persistence_)
				* mac_->slots2bytes (ndx, it->level_, true);

		// frame persistence of request
//		neigh_[ndx][s].pers_in_ = it->persistence_;
		neigh_[ndx][s].level_in_ = it->level_;

		// otherwise, update the status of the req_in_, in bytes
		// we assume that the persistence_ is not 'forever'
		// we assume that bandwidth requests are not canceled
		neigh_[ndx][s].req_in_ += requested;

		Stat::put ("wimsh_req_in", mac_->index(), requested);

		// if this node is not already in the active list, add it
		if ( neigh_[ndx][s].req_in_ > neigh_[ndx][s].gnt_in_ &&
				! activeList_[s].find (wimax::LinkId(ndx, wimax::IN, s)) )
			activeList_[s].insert (wimax::LinkId(ndx, wimax::IN, s));

		// if we receive an rtPS ReqIE, schedule an uncoordinated MSH-DSCH with a Grant response
		if ( dsch->reserved() ) {
			// get slots for this request
			mac_->bwmanager()->searchTXslot(ndx, true);

			// mark rtpsDschFrame_ to send a DSCH in frame f+1
			// mark unDschState_ to send a grant in frame f+1
			// TODO: this assumes searchTXslot is always capable of locating slots in f+1!
			// searchTXslots should instead return the number of the frame where slots were found
			rtpsDschFrame_[ndx] = mac_->frame() + 1;
			unDschState_[ndx][(mac_->frame() + 1) % HORIZON] = 1;

				if ( WimaxDebug::enabled() ) fprintf (stderr,
						"\t\tscheduling an rtPS Grant DSCH in frame %d\n",
						rtpsDschFrame_[ndx]);
		}
	}
}

void
WimshBwManagerFairRR::schedule (WimshMshDsch* dsch, unsigned int ndx)
{
	if ( WimaxDebug::trace("WBWM::schedule") ) {
		fprintf (stderr, "%.9f WBWM::schedule   [%d]\n", NOW, mac_->nodeId());
		printDataStructures (stderr);
	}

	// if this DSCH is dedicated to rtPS
	if ( dsch->reserved() ) {

		// schedule availabilities into the MSH-DSCH message
		if ( avlAdvertise_ ) availabilities (dsch, wimax::RTPS);

		// confirm granted minislot ranges into the MSH-DSCH message
		confirm (dsch, ndx, wimax::RTPS);

		// add bandwidth grants and requests into the MSH-DSCH message
		requestGrant (dsch, ndx, wimax::RTPS);

		return;
	}

	// else schedule bandwidth into the DSCH message
	const unsigned int neighbors = mac_->nneighs();

	// check for uncoordinated DSCH messages that need to be sent in the control subframe (no slots to send in data subframe)
	for ( ndx = 0 ; ndx < neighbors ; ndx++ ) {
		if ( send_rtps_together_[ndx] ) {

			// schedule availabilities into the MSH-DSCH message
		   if ( avlAdvertise_ ) availabilities (dsch, wimax::RTPS);

		   // confirm granted minislot ranges into the MSH-DSCH message
		   confirm (dsch, ndx, wimax::RTPS);

		   // add bandwidth grants and requests into the MSH-DSCH message
		   requestGrant (dsch, ndx, wimax::RTPS);

		   send_rtps_together_[ndx] = false;
		}
	}

	// normal dsch message
	// schedule availabilities into the MSH-DSCH message
	if ( avlAdvertise_ ) availabilities (dsch, 0);

	for ( int s = wimax::UGS ; s >= wimax::BE ; s-- ) {
		if ( s == wimax::RTPS ) continue;

		// confirm granted minislot ranges into the MSH-DSCH message
		confirm (dsch, ndx, s);

		// add bandwidth grants and requests into the MSH-DSCH message
		requestGrant (dsch, ndx, s);
	}

}

void
WimshBwManagerFairRR::availabilities (WimshMshDsch* dsch, unsigned int serv)
{
	if ( serv == wimax::RTPS ) {

		// add as many availabilities as possible
		while ( ! availabilities_[1].empty() ) {

			// if there is not enough space to add an availability, stop now
			if ( dsch->remaining() < WimshMshDsch::AvlIE::size() ) break;
			WimshMshDsch::AvlIE avl = availabilities_[1].front();
			availabilities_[1].pop_front();

			// schedule the availability, unless it contains stale information
			// we assume that the persistence is not 'forever'
			// we ignore cancellations (ie. persistence = 'cancel')
			if ( mac_->frame() <=
					avl.frame_ + WimshMshDsch::pers2frames(avl.persistence_) ) {
				dsch->add (avl);
			}
		}
	} else {

		// add as many availabilities as possible for BE, NRTPS and UGS services queue
		while ( ! availabilities_[0].empty() ) {

			// if there is not enough space to add an availability, stop now
			if ( dsch->remaining() < WimshMshDsch::AvlIE::size() ) break;
			WimshMshDsch::AvlIE avl = availabilities_[0].front();
			availabilities_[0].pop_front();

			// schedule the availability, unless it contains stale information
			// we assume that the persistence is not 'forever'
			// we ignore cancellations (ie. persistence = 'cancel')
			if ( mac_->frame() <=
					avl.frame_ + WimshMshDsch::pers2frames(avl.persistence_) ) {
				dsch->add (avl);
			}
		}
		// add as many availabilities as possible for RTPS queue
		while ( ! availabilities_[1].empty() ) {

			// if there is not enough space to add an availability, stop now
			if ( dsch->remaining() < WimshMshDsch::AvlIE::size() ) break;
			WimshMshDsch::AvlIE avl = availabilities_[1].front();
			availabilities_[1].pop_front();

			// schedule the availability, unless it contains stale information
			// we assume that the persistence is not 'forever'
			// we ignore cancellations (ie. persistence = 'cancel')
			if ( mac_->frame() <=
					avl.frame_ + WimshMshDsch::pers2frames(avl.persistence_) ) {
				dsch->add (avl);
			}
		}
	}

	// add pending grants in the grantWaiting_ queue
	while ( ! grantWaiting_[0].empty() ) {
		// if there isn't enough space to add a pending grant
		if ( dsch->remaining() < WimshMshDsch::GntIE::size() ) break;

		// get the first pending grant
		WimshMshDsch::GntIE gnt = grantWaiting_[0].front();
		grantWaiting_[0].pop_front();

		// add the grant to the MSH-DSCH message
		dsch->add (gnt);
		//dsch->gntCompact();
	}
}

void
WimshBwManagerFairRR::requestGrant (WimshMshDsch* dsch,
		unsigned int ndx, unsigned int serv)
{
	// breakpoint triggers
	unsigned int tnode = mac_->nodeId();

	const unsigned int neighbors = mac_->nneighs();

	// ndx parameter is only used for rtPS

	//
	// we do not want to add request IE directly to the MSH-DSCH message
	// because doing so may unnecessarily increase the overhead (i.e.
	// the MSH-DSCH size) and the approximation error to bytes into
	// minislots multiples
	//
	// thus, we keep a counter of the bytes that we want to request
	// and an explicit variable which stores the remaining amount of
	// bytes in the MSH-DSCH message. The latter is incremented only
	// when a request to a new neighbor is added (checked via the
	// neighReq array)
	//
	// to do so, we also need to compute the maximum amount of bytes
	// that can be requested by a neighbor, which depends on the
	// burst profile currently used. If the amount of bytes requested
	// overflows this value, then we immediately add a new request
	// IE to the MSH-DSCH and update the remaining number of bytes
	// accordingly
	//

	// number of bytes that can still be allocated into the MSH-DSCH message
	//unsigned int reqIeOccupancy = 0;

	// the i-th element is true if we requested bandwidth to neighbor i
	std::vector<bool> neighReq (neighbors, false);

	// array of vectores that stores the number of bytes requested to neighbor i of type service j
	std::vector< std::vector< unsigned int > > neighReqBytes;
	neighReqBytes.resize (neighbors);
	for ( unsigned int i = 0 ; i < neighbors ; i++ ) {
		neighReqBytes[i].resize (4);
		for ( unsigned int j = 0 ; j < 4 ; j++ ) neighReqBytes[i][j] = 0;
	}

	// the i-th element stores the maximum number of bytes that can be
	// requested to neighbor i in a single IE
	std::vector<unsigned int> neighReqMax (neighbors);
	for ( unsigned int i = 0 ; i < neighbors ; i++ ) {
		neighReqMax[i] =
			  mac_->phyMib()->slotPerFrame()
			* mac_->phyMib()->symPerSlot ()
			* WimshMshDsch::pers2frames (WimshMshDsch::FRAME128)
			* mac_->alpha (i);
	}

	// create an array of grantFitDescriptor's to be used during the
	// grant procedure to keep the current channel/frame/slot to analyze
	std::vector<grantFitDesc> grantFitStatus (neighbors);

	// reset input counters
	for ( unsigned int n = 0 ; n < neighbors ; n++ ) {

		if ( neigh_[n][serv].gnt_in_ > neigh_[n][serv].req_in_ )
				neigh_[n][serv].gnt_in_ = neigh_[n][serv].req_in_;

		if ( neigh_[n][serv].cnf_in_ > neigh_[n][serv].gnt_in_ )
				neigh_[n][serv].cnf_in_ = neigh_[n][serv].gnt_in_;

		if ( neigh_[n][serv].req_in_ == neigh_[n][serv].cnf_in_ &&
					neigh_[n][serv].cnf_in_ == neigh_[n][serv].gnt_in_ ) {
				neigh_[n][serv].req_in_ -= neigh_[n][serv].gnt_in_;
				neigh_[n][serv].gnt_in_ = 0;
				neigh_[n][serv].cnf_in_ = 0;
		}
	}

	//
	// request bandwidth on a round robin manner
	// stop when one of the following occurs:
	// 1. the active list is empty
	// 2. there is not enough room in the MSH-DSCH to add a request/grant
	// 3. none of the links (both input and output) are eligible for service
	// 4. the round robin iterator points to a link descriptor
	//    whose deficit cannot be incremented, because doing so would
	//    overflow the maximum deficit amount, if specified
	//
	// to check 3. we use an integer number storing the index of the
	// first neighbor that was not eligible for service; if an eligible
	// link is encountered by the round-robin pointer through its walk,
	// that variable is set as not valid, to indicate that at least one link
	// is eligible
	//
	// NOTE: condition 4. is only enabled if fairGrant_ is trued
	//       otherwise, the maxDeficit_ acts as a threshold (if > 0)
	//
	unsigned int ineligible = 0;    // only meaningful with ineligibleValid true
	bool ineligibleValid = false;

	// update the deadlock detection timer
	if ( deficitOverflow_ ) ++ddTimer_;

	//----------------------------------------------//
	// we GRANT bandwidth							//
	//----------------------------------------------//
	while ( ! activeList_[serv].empty() ) {									// 1.

		if ( serv == wimax::RTPS ) {

			if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf (stderr,
					"\tMSH-DSCH is for %s\n",
					( dsch->grant() ) ? "GNT" : "REQ");

			// TODO: ding
			if ( ! dsch->grant() )
				break;

			if ( ! activeList_[serv].find (wimax::LinkId(ndx, wimax::IN, serv)) )
				break;

			while ( activeList_[serv].current().ndx_ != ndx )
				activeList_[serv].move();

		} else {
			// get current link information
			ndx = activeList_[serv].current().ndx_;				// index
		}

		unsigned int dst = mac_->ndx2neigh (ndx);				// NodeID

		// check if there are not any more eligible links
		if ( ineligibleValid && ineligible == ndx ) break;					// 3.

		// stop if there is not enough room in this MSH-DSCH to add a grant
		if ( dsch->remaining() < WimshMshDsch::GntIE::size() )
			break;															// 2.

		// get the handshake time of the neighbor currently served
		unsigned int Hdst = handshake (dst);

		// alias for the deficit counter and other variables
		unsigned int& granted   = neigh_[ndx][serv].gnt_in_;
		unsigned int& requested = neigh_[ndx][serv].req_in_;
		unsigned int& level = neigh_[ndx][serv].level_in_;		// slots * persistence

		// number of pending bytes
		unsigned int total_req = requested - granted;

		// minislots requested per frame
		unsigned int req_now = mac_->slots2bytes (ndx, level, true);
		unsigned int bytes_req = req_now;
		int slots_req = level;

		// pending bytes are decremented during cycle below
		unsigned int pending = total_req;

		//
		// grant until one of the following occurs:
		// .....?!"?"...
		//
		bool room = true, frame_room = true;
		unsigned int minFrame = ( serv == wimax::RTPS ) ? 2 : Hdst;
		unsigned int maxHorizon;
		if ( serv == wimax::UGS ) maxHorizon = HORIZON;
		else maxHorizon = (HORIZON / 4);
		unsigned int nrtPS_slots = nrtpsMinSlots_;

		for ( unsigned int h = minFrame ; h < minFrame + 1 ; h++ ) {

			// get a new grant information element
			WimshMshDsch::GntIE gnt;

			// grant up to 'deficit' bytes within the time horizon
			gnt = grantFit (ndx,
					bytes_req,						// max number of bytes (slots * persistence)
//					level,							// demand level
//					pers,							// demand persistences
					mac_->frame() + h,				// first eligible frame
					room,							// room for more grant entries
					frame_room,
					grantFitStatus[ndx],			// current grant status
					serv,							// traffic class
					dsch);

			// if the minislot range is empty, then it was not possible
			// to grant more bandwidth in this frame
			if ( gnt.range_ == 0 ) {
				slots_req = level;
				bytes_req = req_now;
				continue;
			}

			//if ( WimaxDebug::enabled() ) fprintf (stderr,
			//		"slots_req %d frame %d h %d\n", slots_req , mac_->frame() + h , h);
			if ( gnt.range_ < level ) {
				slots_req -= gnt.range_ - 1;
				if ( serv == wimax::NRTPS && !frame_room ) slots_req = nrtpsMinSlots_ -
						(level - slots_req);
				bytes_req = mac_->slots2bytes (ndx, slots_req, true);
				if ( slots_req <= 1 ) {
					slots_req = level;
					bytes_req = req_now;
				} else h--;
			}

			//if ( WimaxDebug::enabled() ) fprintf (stderr,
			//		"slots_req %d frame %d h %d\n", slots_req , mac_->frame() + h , h);

			// collect the average grant size, in minislots
			Stat::put ("wimsh_gnt_size", mac_->index(), gnt.range_);

			// number of bytes granted in this frame
			unsigned int bgnt = WimshMshDsch::pers2frames(gnt.persistence_) * mac_->slots2bytes (ndx, gnt.range_, true);

			//if ( WimaxDebug::enabled() ) fprintf (stderr, "number of bytes granted %d\n", bgnt);

			// update the number of bytes still needed
			// since bandwidth is granted in terms of minislots, then it
			// is possible that are granted more bytes than needed
			// in this case, we do not count the surplus allocation
			// and just reset the needed variable to zero
			pending = ( pending > bgnt ) ? ( pending - bgnt ) : 0;

			// update the granted counter
			granted += bgnt;
			Stat::put ("wimsh_gnt_in", mac_->index(), bgnt);

			//if ( WimaxDebug::enabled() ) fprintf (stderr, "granted %d\n",granted);

			realGrantStart (ndx, gnt.frame_, gnt.start_, gnt.range_, gnt.channel_, gnt);

			if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf (stderr,
					"\tgranting: src %d dst %d frame %d start %d range %d pers %d serv %d\n",
					mac_->nodeId(), mac_->ndx2neigh(ndx), gnt.frame_, gnt.start_, gnt.range_, gnt.persistence_, gnt.service_);

			// add the grant to the MSH-DSCH message, if no room save it
			// in grantWaiting_ list and transmit it on next opportunity
			if ( dsch->remaining() > WimshMshDsch::GntIE::size() && room == true) {
				dsch->add (gnt);
			}
			else
				grantWaiting_[0].push_back (gnt);

			// set the granted slots as unavailable for reception
			if ( serv == wimax::UGS )
				setSlots (busy_UGS_, gnt.frame_, WimshMshDsch::pers2frames(gnt.persistence_),
						gnt.start_, gnt.range_, true);
			else {
				if ( serv == wimax::NRTPS && nrtPS_slots > 0) {
					unsigned int slots = ( gnt.range_ > nrtPS_slots ) ? nrtPS_slots : gnt.range_;

					setSlots (busy_NRTPS_, gnt.frame_, WimshMshDsch::pers2frames(gnt.persistence_),
							gnt.start_, slots, true);

					nrtPS_slots = nrtPS_slots - gnt.range_;
				}

				setSlots (busy_, gnt.frame_, WimshMshDsch::pers2frames(gnt.persistence_),
						gnt.start_, gnt.range_, true);
			}

			setSlots (service_, gnt.frame_, WimshMshDsch::pers2frames(gnt.persistence_),
					gnt.start_, gnt.range_, serv);
		}

		//if ( granted < total_req )
		//	if ( WimaxDebug::enabled() ) fprintf (stderr, "!!nao garanti tudo\n");

		activeList_[serv].erase();
	}

	//-------------------------------------------------//
	// we REQUEST bandwidth							   //
	//-------------------------------------------------//

	// add to the MSH-DSCH message the request IEs that have been
	// accounted for during the request/grant process above

	// if service is rtPS, run the following code only for the 'ndx' passed to this function
	// otherwise, run for all of this node's neighbors
	unsigned int ndxMin = ( serv == wimax::RTPS ) ? ndx : 0;
	unsigned int ndxMax = ( serv == wimax::RTPS ) ? (ndx + 1) : neighbors;

	for ( unsigned int ndx = ndxMin ; ndx < ndxMax ; ndx++ ) {
			// flag to check whether there is an estimate for this flow's bandwidth needs
			bool reqTraffic = ( mac_->scheduler()->cbrQuocient (ndx, serv) > 0 ) ? true : false;

			// to speed up rtPS, we do not wait for cbr_ to have an estimate, and request slots for the traffic in backlog
			bool rtPShurry = false;
			if( serv == wimax::RTPS && !reqTraffic && mac_->scheduler()->cbrBytes (ndx, serv) > 0 )
				rtPShurry = true;

			if( startHorizon_[ndx][serv] && (reqTraffic || rtPShurry) ) {
				// create a request IE
				WimshMshDsch::ReqIE ie;
				ie.nodeId_ = mac_->ndx2neigh (ndx);

				unsigned int quocient, req_bytes, req_slots;
				if( !rtPShurry ) {
					// get this class' bandwidth estimates
					quocient = mac_->scheduler()->cbrQuocient (ndx, serv);
	//				unsigned int extquocient = mac_->scheduler()->cbrExtQuocient (ndx, serv); // to outside the neighborhood
					// calculate number of bytes requested per frame
					req_bytes = (unsigned int) ( ( quocient * mac_->phyMib()->frameDuration() ) / 8 );
	//				unsigned int req_extbytes = ( extquocient * mac_->phyMib()->frameDuration() ) / 8;
					// and the number of slots
					req_slots = mac_->bytes2slots (ndx, req_bytes, true);
	//				unsigned int req_extslots = mac_->bytes2slots (ndx, req_extbytes, true);
				} else {
					req_bytes = mac_->scheduler()->cbrBytes (ndx, serv);
					req_slots = mac_->bytes2slots (ndx, req_bytes, true);
					if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf(stderr,
							"\tfasttracking rtPS: bytes %d slots %d\n",
							req_bytes, req_slots);
				}


				/* the next code tries to balance duplex traffic needs
				 * - check for fwdquocient_[] needs
				 * - compute minislot requirements for those needs, according to burst profile to ndx
				 * - get a list of available slots to transmit to ndx
				 * - balance between:
				 *   - minislots reserved for incoming fwdquocient (estimate)
				 *   - minislots required for outgoing traffic (estimate)
				 *   - available minislots to send to ndx
				 * - and then cancel reservations as necessary
				 */

				// for the target node ndx, we look at all the traffic being forwarded from other neighbors
				for( unsigned sndx=0; sndx < neighbors; sndx++ ) {
					if( sndx == ndx ) continue; // no narcissism

					// get how much bw is required
					unsigned fwdQuocient = mac_->scheduler()->cbrFwdQuocient(sndx, ndx, serv);
					// no demands
					if( fwdQuocient == 0 ) continue;

					// slots per frame
					unsigned int N = mac_->phyMib()->slotPerFrame();
					// we assume a single channel (caution...)
					unsigned int ch = 0;
					// frame to start looking for available slots (we use +10, could be +4)
					unsigned int F = (mac_->frame() + 10) % HORIZON;

					// convert quocient to bytes
					unsigned int fwdBytes = (unsigned int) ( ( fwdQuocient * mac_->phyMib()->frameDuration() ) / 8 );
					// convert bytes to minislots
					unsigned int fwdSlots = mac_->bytes2slots (ndx, fwdBytes, true);
					// limits, just in case the estimator is faulty
					(fwdSlots > N) ? fwdSlots = N : fwdSlots;

					// now estimate how many minislots this neighbor is already using to send the data to us
					unsigned int bwdSlots = mac_->bytes2slots (sndx, fwdBytes, true);
					// just for checks, get the number of minislots really given by this node to sndx/serv
					unsigned slotCount = 0;
					for(unsigned i = 0; i < N; i++)
					if(src_[F][i] == mac_->ndx2neigh(sndx) && service_[F][i]==serv)
						slotCount++;

					// uncomment to print src_ to stderr
//					for(unsigned i=0; i < N; i++) std::cerr << src_[F][i] << " ";
//					fprintf(stderr, "\n");

					// naturally, our estimate can't be larger than the real number of slots allocated
					if (bwdSlots > slotCount) {
						// a measure of how much the estimator overshoot (only works with single flows...)
						float factor = (float)slotCount/(float)bwdSlots;
						bwdSlots = slotCount;
						// correct forward estimates also, then
						fwdSlots = (unsigned) (fwdSlots*factor);
						if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf(stderr,
								"\tfwdtraffic: correction factor %f fwdSlots %d\n",
								factor, fwdSlots);
					}

					// evaluate how many slots are available for this need

					// map with available slots (true -> unavailable, false -> available)
					// !unconfirmed + !busy + !self_tx_unavl
					// note: we should consider the destination node's unavailabilities to receive
					std::bitset<MAX_SLOTS> map =
						unconfirmedSlots_[F] | unconfirmedSlots_UGS_[F] | unconfirmedSlots_NRTPS_[F] |
						busy_[F] | busy_UGS_[F] | busy_NRTPS_[F] |
//						neigh_rx_unavl_[ndx][ch][F] | neigh_rx_unavl_UGS_[ndx][ch][F] | neigh_rx_unavl_NRTPS_[ndx][ch][F] |
						self_tx_unavl_[ch][F] | self_tx_unavl_UGS_[ch][F] | self_tx_unavl_NRTPS_[ch][F];

					// uncomment to print map to stderr
//					for(unsigned i = 0; i<N;i++) std::cerr << map[i];
//					fprintf(stderr, "\n");

					// minislot start and range
					unsigned int mstart, mrange;
					for ( mstart=0; map[mstart] != false && mstart < N; mstart++ ); // get the start point
					if( mstart != N ) { 	// if at least one slot was found
						// get the available range
						for ( mrange = 1; (map[mstart+mrange] == false) && (mstart+mrange < N); mrange++ );
					} else {
						mstart = 0; mrange = 0;
					}

					if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf (stderr,
							"\tfwdtraffic: ndx %d needing %d slots to ndx %d, using %d/%d slots, %d available for fwd [%d:%d]\n",
							sndx, fwdSlots, ndx, bwdSlots, mac_->phyMib()->slotPerFrame(), mrange, mstart, mstart+mrange);

					// evaluate how many slots should be canceled to have same number of slots available to send and receive
					if( fwdSlots > mrange ) {
						int balance=0, cancel=0;
						balance = (bwdSlots + mrange)/2;
						cancel = bwdSlots - balance;

						// get the reservation's end point
						unsigned rsvEnd = 0;
						for( ; rsvEnd < N && src_[F][rsvEnd] != mac_->ndx2neigh(sndx) ; rsvEnd++);
						for( ; rsvEnd < N && src_[F][rsvEnd] == mac_->ndx2neigh(sndx) ; rsvEnd++);

						// to cancel a request, we create a GrantIE with persistence level CANCEL
						// cancelations are directed, thus the need for a GrantIE and nodeId_
						WimshMshDsch::GntIE cancelIE;
						// send the cancel to the traffic source
						cancelIE.nodeId_ = mac_->ndx2neigh(sndx);
						// frame to start canceling reservation (we use +10, could be +4)
						cancelIE.frame_ = mac_->frame() + 10;
						// minislot start (we cancel the end part of the reservation)
						cancelIE.start_ = rsvEnd - cancel;
						// minislot range
						cancelIE.range_ = cancel;
						// irrelevant
						cancelIE.fromRequester_ = 0;
						// set to CANCEL
						cancelIE.persistence_ = WimshMshDsch::CANCEL;
						// channel number (caution, again...)
						cancelIE.channel_ = 0;
						// service class
						cancelIE.service_ = serv;

						if ( WimaxDebug::trace("WBWM::requestGrant") ) fprintf (stderr,
								"\t\tbalance %d cancel %d IE frame %d start %d range %d ch %d serv %d\n",
								balance, cancel, cancelIE.frame_, cancelIE.start_, cancelIE.range_, cancelIE.channel_, cancelIE.service_);

						// the cancel IE can go right into this DSCH
						dsch->add (cancelIE);

						// now we cancel the reservations locally, starting in cancelIE.frame_
						if ( serv == wimax::UGS ) {
							setSlots (busy_UGS_, 					cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, false);
							setSlots (self_rx_unavl_UGS_[ch], 		cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, false);
							setSlots (neigh_tx_unavl_UGS_[sndx][ch],cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, false);

							setSlots (src_, 	cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, UINT_MAX);
							setSlots (grants_, 	cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, false);
							setSlots (service_, cancelIE.frame_, 128, cancelIE.start_, cancelIE.range_, 9);

							// also, create an AvailabilityIE advertising the freed slots

							WimshMshDsch::AvlIE avlIE;
							avlIE.frame_ = cancelIE.frame_;
							avlIE.start_ = cancelIE.start_;
							avlIE.range_ = cancelIE.range_;
							avlIE.direction_ = WimshMshDsch::TX_AVL;
							avlIE.persistence_ = WimshMshDsch::FRAME128;
							avlIE.channel_ = cancelIE.channel_;
							avlIE.service_ = cancelIE.service_;

							dsch->add (avlIE);
						} else {
							// for classes that request bandwidth periodically, just send the cancel IE
							// the sender is then responsible for limiting its requests accordingly
						}
					}
				}	// fwdtraffic handling ends here

				// fill IE with dst nodeid, demand level, demand persistence, service class
				ie.level_ = req_slots + 3;
				if (ie.level_ > mac_->phyMib()->slotPerFrame()) // if we ask for too many slots per frame
					ie.level_ = mac_->phyMib()->slotPerFrame(); // crop

				switch(serv){
					case wimax::UGS:
						ie.persistence_ = WimshMshDsch::FRAME128; // should be FOREVER
						nextFrame_[ndx][serv] = UINT_MAX;
						break;
					default:
						ie.persistence_ = WimshMshDsch::FRAME32;
						nextFrame_[ndx][serv] = mac_->frame() + 32;
				}
				ie.service_ = serv;

				// insert the IE into the MSH-DSCH message
				dsch->add (ie);

				// TODO: document this (startHorizon)
				startHorizon_[ndx][serv] = false;

				// update request out (requested bytes per frame * persistence)
				neigh_[ndx][serv].req_out_ = mac_->slots2bytes (ndx, ie.level_, true) * WimshMshDsch::pers2frames(ie.persistence_);

				if ( WimaxDebug::trace("WBWM::requestGrant") ) {
					fprintf (stderr,
						"\trequesting: src %d dst %d bytes %d level %d pers %d serv %d\n",
						mac_->nodeId(), mac_->ndx2neigh(ndx), req_bytes, ie.level_, ie.persistence_, ie.service_);

					fprintf (stderr,
						"\t\tbased on estimate: quocient %d bytes %d slots %d\n",
						quocient, req_bytes, req_slots);
				}

			} else {
				// no requests were made
				neigh_[ndx][serv].req_out_ = 0;
			}

			// update the requested counter
			Stat::put ("wimsh_req_out", mac_->index(), neigh_[ndx][serv].req_out_);
	}
}

void
WimshBwManagerFairRR::confirm (WimshMshDsch* dsch, unsigned int nodeid, unsigned int serv)
{
	// breakpoint triggers
	unsigned int tnode = mac_->nodeId();

	if ( serv == wimax::RTPS && dsch->grant() )
		return;

	unsigned int nqueue = ( serv == wimax::RTPS ) ? 1 : 0;
	unsigned int node = ( serv == wimax::RTPS ) ? nodeid : 0;

	// confirm as many grants as possible
	while ( ! unconfirmed_[nqueue][node].empty() ) {
		// if there is not enough space to add a confirmation to this
		// grant, then stop now
		if ( dsch->remaining() < WimshMshDsch::GntIE::size() ) break;

		// get the first unconfirmed grant
		WimshMshDsch::GntIE gnt = unconfirmed_[nqueue][node].back();
		unconfirmed_[nqueue][node].pop_back();

		//
		// we assume that the persistence is not 'forever'
		// we ignore cancellations (ie. persistence = 'cancel')
		//

		// get the service class of this grant
		unsigned int s = gnt.service_;

		// get the start frame number and range
		unsigned int fstart;  // frame start number
		unsigned int frange;  // frame range

		// convert the <frame, persistence> into the actual values
		realPersistence (gnt.frame_, gnt.persistence_, fstart, frange);

		if ( s == wimax::UGS ) {
			fstart = gnt.frame_;
			frange = WimshMshDsch::pers2frames(gnt.persistence_);
		}

		// get the start minislot number and range
		unsigned int mstart = gnt.start_;  // minislot start number
		unsigned int mrange = gnt.range_;  // minislot range

		// granted/confirmed bytes
		unsigned int confirmed = 0;

		// get the index of this neighbor
		unsigned int ndx = mac_->neigh2ndx (gnt.nodeId_);


		bool room = true;

		// confirm as many slots as possible
		for ( unsigned int h = fstart ; h < fstart + 1 ; h++ ) {

			// find the first block of available slots
			confFit (h, mstart, mrange, gnt, room, s, dsch);

			// if there are not anymore blocks, terminate
			if ( gnt.range_ == 0 ) continue;

			// collect the average confirmed grant size, in minislots
			Stat::put ("wimsh_cnf_size", mac_->index(), gnt.range_);

			if ( WimaxDebug::trace("WBWM::confirm") ) fprintf (stderr,
					"\tconfirming: src %d dst %d frame %d start %d range %d pers %d serv %d\n",
					mac_->nodeId(), mac_->ndx2neigh(ndx), gnt.frame_, gnt.start_, gnt.range_, gnt.persistence_, gnt.service_);

			// schedule the grant as a confirmation, if no room save it
			// in grantWaiting_ list and transmit it on next opportunity
			if ( dsch->remaining() > WimshMshDsch::GntIE::size() && room == true)
				dsch->add (gnt);
			else
				grantWaiting_[0].push_back (gnt);

			// convert to the actual values of <frame, range>
			unsigned int fs;  // frame start
			unsigned int fr;  // frame range
			realPersistence (gnt.frame_, gnt.persistence_, fs, fr);

			if ( s == wimax::UGS ) {
				fs = gnt.frame_;
				fr = WimshMshDsch::pers2frames(gnt.persistence_);
			}

			// compute the number bytes confirmed
			confirmed += (WimshMshDsch::pers2frames(gnt.persistence_)
							* mac_->slots2bytes (ndx, gnt.range_, true));

			// mark the minislots
			if ( s == wimax::UGS )
				setSlots (busy_UGS_, fs, fr, gnt.start_, gnt.range_, true);
			else {
				if ( s == wimax::NRTPS ) {
					unsigned int slots = ( gnt.range_ > nrtpsMinSlots_ ) ?
							nrtpsMinSlots_ : gnt.range_;
					setSlots (busy_NRTPS_, fs, fr, gnt.start_, slots, true);
				}
				setSlots (busy_, fs, fr, gnt.start_, gnt.range_, true);
			}
			setSlots (grants_, fs, fr, gnt.start_, gnt.range_, true);
			setSlots (dst_, fs, fr, gnt.start_, gnt.range_, gnt.nodeId_);
			setSlots (src_, fs, fr, gnt.start_, gnt.range_, 999);
			setSlots (service_, fs, fr, gnt.start_, gnt.range_, s);
			setSlots (channel_, fs, fr, gnt.start_, gnt.range_, gnt.channel_);
		}


		// update the number of bytes confirmed
		neigh_[ndx][serv].cnf_out_ += confirmed;
		Stat::put ("wimsh_cnf_out", mac_->index(), confirmed);

		// we enforce the number of granted bytes to be smaller than
		// that of granted bytes
		neigh_[ndx][serv].cnf_out_ =
			( neigh_[ndx][serv].cnf_out_ < neigh_[ndx][serv].gnt_out_ ) ?
			neigh_[ndx][serv].cnf_out_ : neigh_[ndx][serv].gnt_out_;              // XXX

		// remove the confirmed bytes from the data structures
		//if ( neigh_[ndx][serv].req_out_ < neigh_[ndx][serv].cnf_out_ ) abort();  // XXX
		neigh_[ndx][serv].gnt_out_ -= neigh_[ndx][serv].cnf_out_;                // XXX
		neigh_[ndx][serv].req_out_ -= neigh_[ndx][serv].cnf_out_;                // XXX
		neigh_[ndx][serv].cnf_out_ = 0;                                    // XXX

	}
}

void
WimshBwManagerFairRR::invalidate (unsigned int F)
{
	// compute the number of slots in the current frame
	// that could have been used to transmit date for measurement purposes
	const unsigned int unused =
		mac_->phyMib()->slotPerFrame() - (busy_[F].count() + busy_UGS_[F].count());
	Stat::put ("wimsh_unused_a", mac_->index(), unused);
	Stat::put ("wimsh_unused_d", mac_->index(), unused);

	if ( WimaxDebug::trace("WBWM::invalidate") ) fprintf (stderr,
			"%.9f WBWM::invalidate [%d] unused %d\n", NOW, mac_->nodeId(), unused);

	// reset to default values all data structures of the last frame
	for ( unsigned int ngh = 0 ; ngh < mac_->nneighs() ; ngh++)
		for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
			neigh_tx_unavl_[ngh][ch][F].reset();
			neigh_tx_unavl_NRTPS_[ngh][ch][F].reset();
		}

	for ( unsigned int ch = 0 ; ch < mac_->nchannels() ; ch++ ) {
		self_rx_unavl_[ch][F].reset();
		self_tx_unavl_[ch][F].reset();
		self_rx_unavl_NRTPS_[ch][F].reset();
		self_tx_unavl_NRTPS_[ch][F].reset();
	}
	busy_[F].reset();
	busy_NRTPS_[F].reset();
	unconfirmedSlots_[F].reset();
	unconfirmedSlots_NRTPS_[F].reset();
	WimshBwManager::invalidate (F);
}

void
WimshBwManagerFairRR::cancel_Requester (unsigned int ndx,
		unsigned char s, WimshMshDsch* dsch)
{
	if ( WimaxDebug::enabled() ) fprintf (stderr,"requester vou cancelar o servico UGS\n");
	WimaxNodeId dst = mac_->ndx2neigh (ndx);
	unsigned int fs = mac_->frame();
	unsigned int F;

	// send UGS cancel ReqIE to receiver
	WimshMshDsch::ReqIE req;
	req.nodeId_ = dst;
	req.level_ = 0;
	req.persistence_ = WimshMshDsch::CANCEL;
	req.service_ = s;
	dsch->add (req);

	// create AvlIE to inform sender's neighbours
	for ( unsigned int f = fs ; f < ( fs + HORIZON )  ; f++ ) {
		F = f % HORIZON;
		for ( unsigned int i = 0 ; i < MAX_SLOTS ; i++ ) {

			if ( service_[F][i] == s && dst_[F][i] == dst ) {

				WimshMshDsch::AvlIE avl;
				avl.frame_ = f;
				avl.start_ = i;
				avl.direction_ = WimshMshDsch::RX_AVL;	// distinguish from the granter AvlIE
				avl.persistence_ = WimshMshDsch::FRAME1;
				avl.channel_ = channel_[F][i];
				avl.service_ = s;

				for ( ; i < MAX_SLOTS && service_[F][i] == s &&
						dst_[F][i] == dst ; i++ ) { }

				avl.range_ = i - avl.start_;

				if ( dsch->remaining() > WimshMshDsch::AvlIE::size() )
					dsch->add (avl);
				else
					availabilities_[0].push_back (avl);

				// clear data structures busy with UGS service
				setSlots (busy_UGS_, avl.frame_, 1,
						avl.start_, avl.range_, false);
				setSlots (grants_, avl.frame_, 1,
						avl.start_, avl.range_, false);
				setSlots (service_, avl.frame_, 1,
						avl.start_, avl.range_, 9);
				setSlots (dst_, avl.frame_, 1,
						avl.start_, avl.range_, 999);
				setSlots (channel_, avl.frame_, 1,
						avl.start_, avl.range_, false);
				setSlots (unconfirmedSlots_UGS_, avl.frame_, 1,
						avl.start_, avl.range_, false);
			}
		}
	}
}

void
WimshBwManagerFairRR::cancel_Granter (unsigned int ndx,
		unsigned char s)
{
	if ( WimaxDebug::enabled() ) fprintf (stderr,"granter vou cancelar o servico UGS\n");
	WimaxNodeId src = mac_->ndx2neigh (ndx);
	unsigned int fs = mac_->frame() + 10;	// !!
	unsigned int F;

	// create AvlIE to inform granter's neighbours
	for ( unsigned int f = fs ; f < ( fs + HORIZON )  ; f++ ) {
		F = f % HORIZON;
		for ( unsigned int i = 0 ; i < MAX_SLOTS ; i++ ) {

			if ( service_[F][i] == s && src_[F][i] == src ) {

				WimshMshDsch::AvlIE avl;
				avl.frame_ = f;
				avl.start_ = i;
				avl.direction_ = WimshMshDsch::TX_AVL;
				avl.persistence_ = WimshMshDsch::FRAME1;
				avl.channel_ = channel_[F][i];
				avl.service_ = s;

				for ( ; i < MAX_SLOTS && service_[F][i] == s &&
						src_[F][i] == src ; i++ ) { }

				avl.range_ = i - avl.start_;

				availabilities_[0].push_back (avl);

				// clear data structures busy with UGS service
				setSlots (busy_UGS_, avl.frame_, 1,
						avl.start_, avl.range_, false);
				setSlots (src_, avl.frame_, 1,
						avl.start_, avl.range_, 999);
				setSlots (service_, avl.frame_, 1,
						avl.start_, avl.range_, 9);
				setSlots (channel_, avl.frame_, 1,
						avl.start_, avl.range_, false);
			}
		}
	}
}

void
WimshBwManagerFairRR::realPersistence (
		unsigned int start, WimshMshDsch::Persistence pers,
		unsigned int& realStart, unsigned int& range)
{
	range = WimshMshDsch::pers2frames(pers);

	// if the start frame of the grant is smaller than the
	// current frame number, then some (or all) the information is stale
	range -= ( start >= mac_->frame() ) ? 0 : ( mac_->frame() - start );

	if ( range < 0 ) range = 0;

	// the start time is the largest value between start and the current frame
	realStart = ( start >= mac_->frame() ) ? start : mac_->frame();
}

WimshMshDsch::GntIE
WimshBwManagerFairRR::grantFit (
		unsigned int ndx, unsigned int bytes,
		unsigned int frame,
		bool& room, bool& frame_room,
		grantFitDesc& status,
		unsigned int serv_class, WimshMshDsch* dsch)
{
	// new grant to be returned (and then added to the MSH-DSCH)
	WimshMshDsch::GntIE gnt;
	gnt.nodeId_ = mac_->ndx2neigh (ndx);

	// TODO: GrantFit isn't passed the persistence level
	WimshMshDsch::Persistence persistence;
//	if ( req-persistence == WimshMshDsch::FOREVER )
	if ( serv_class == wimax::UGS )
		persistence = WimshMshDsch::frames2pers(HORIZON);
	else
		persistence = WimshMshDsch::frames2pers(HORIZON / 4);

	// number of minislots per frame
	unsigned int N = mac_->phyMib()->slotPerFrame();

	// number of channels
	unsigned int C = mac_->nchannels();

	// number of request slots
	unsigned int nSlots = mac_->bytes2slots (ndx, bytes, true);

	// search the frame and first minislot available for grant
	// set the actual frame number within the frame horizon
	unsigned int F = (frame + 10) % HORIZON;

	// pick up a random channel, if required
	unsigned int ch = 0;
	if ( grantFitRandomChannel_ ) ch = grantFitRng.uniform ((int)C);

	// nrtps pending slots
	unsigned int count = 0;

	// for each channel
	for ( unsigned int c = 0 ; c < C ; c++ ) {

		// get a bitset which represents the grant unavailabilities	(1 == unavailable, 0 == available)
		std::bitset<MAX_SLOTS> map =
		  unconfirmedSlots_[F] | unconfirmedSlots_UGS_[F] | unconfirmedSlots_NRTPS_[F] |
		  busy_[F] | busy_UGS_[F] | busy_NRTPS_[F] |
		  self_rx_unavl_[ch][F] | self_rx_unavl_UGS_[ch][F] | self_rx_unavl_NRTPS_[ch][F] |
		  neigh_tx_unavl_[ndx][ch][F] | neigh_tx_unavl_UGS_[ndx][ch][F] | neigh_tx_unavl_NRTPS_[ndx][ch][F];

		// for each minislot in the current frame
		for ( unsigned int s = 0 ; s < N ; s++ ) {
			//if ( txBE_[F][serv] == false ) fprintf (stderr, "!!txBE_ = 0 tarma %d slot %d\n", frame ,s  );

			// as soon as a free minislot is found, start the grant allocation
			if ( map[s] == false ) {

				gnt.service_ = serv_class;
				gnt.frame_ = frame;
				gnt.start_ = s;
				gnt.persistence_ = persistence;
				gnt.fromRequester_ = false;
				gnt.channel_ = ch;

				// search for the largest minislot range
				for ( ; s < N && map[s] == false &&
					( s - gnt.start_ ) < nSlots ; s++ ) { }

				gnt.range_ = s - gnt.start_;
				//if ( WimaxDebug::enabled() ) fprintf (stderr,
				//		"gnt.range %d  %d-%d  frame .start_, s, frame);

				// check the minimum number of OFDM symbols per grant
				// unless the number of slots requested is smaller than that
				unsigned int symbols =
					  gnt.range_ * mac_->phyMib()->symPerSlot()
					- mac_->phyMib()->symShortPreamble();

				if ( symbols < minGrant_ ) continue;

				return gnt;
			}

			// frame is full
			if ( s == N - 1 ) {
				if ( WimaxDebug::enabled() ) fprintf (stderr,
						"!1frame totalmente ocupada %d ultimo s %d \n", frame, s);
				if ( serv_class == wimax::BE ) {
					gnt.range_ = 0;
					//if ( WimaxDebug::enabled() ) fprintf (stderr,
					//		"!2 BE: frame totalmente ocupada %d ultimo s %d \n", frame, s);
					return gnt;

				} else {

					// for each minislot in the current frame
					for ( unsigned int m = 0 ; m < N ; m++ ) {

						// borrow bandwidth of BE and nrtPS services
						// but attention to nrtPS minimum slots
						if ( service_[F][m] == 9 || service_[F][m] == wimax::BE ||
								(service_[F][m] == wimax::NRTPS &&
										!(( unconfirmedSlots_NRTPS_[F][m] ||
												busy_NRTPS_[F][m] || self_rx_unavl_NRTPS_[ch][F][m] ||
												neigh_tx_unavl_NRTPS_[ndx][ch][F][m]))) ) {
							map[m] = false;
							count++;
						}
					}

					if (count > 0)
					{
						for ( unsigned int r = 0 ; r < N ; r++ ) {

							// as soon as a  service_[F][m] == 9 || free minislot is found, start the grant allocation
							if ( map[r] == false ) {

								gnt.service_ = serv_class;
								gnt.frame_ = frame;
								gnt.start_ = r;
								gnt.persistence_ = persistence;
								gnt.fromRequester_ = false;
								gnt.channel_ = ch;

								// search for the largest minislot range
								for ( ; r < N && map[r] == false &&
									( r - gnt.start_ ) < nSlots ; r++ ) { }

								gnt.range_ = r - gnt.start_;
								if ( WimaxDebug::trace("WBWM::grantFit") ) fprintf (stderr,
										"%.9f WBWM::grantFit   [%d] range %d (%d-%d) frame %d\n",
										NOW, mac_->nodeId(), gnt.range_, gnt.start_, r, frame);

								unsigned int symbols =
									gnt.range_ * mac_->phyMib()->symPerSlot()
										- mac_->phyMib()->symShortPreamble();

								if ( symbols < minGrant_ ) continue;

								WimshMshDsch::AvlIE avl;
								avl.frame_ = gnt.frame_;
								avl.start_ = gnt.start_;
								avl.direction_ = WimshMshDsch::TX_AVL;
								avl.persistence_ = WimshMshDsch::FRAME128;
								avl.channel_ = ch;
								avl.service_ = (serv_class == wimax::UGS ) ? wimax::RTPS : serv_class; // TODO: if ugs then rtps?
								avl.range_ = gnt.range_;

								if ( dsch->remaining() > WimshMshDsch::AvlIE::size() +
										WimshMshDsch::GntIE::size() )
									dsch->add (avl);
								else {
									if (serv_class == wimax::NRTPS )
										availabilities_[0].push_back (avl);
									else
										availabilities_[1].push_back (avl);

									room = false;
								}

								setSlots (dst_, frame, 128, gnt.start_, gnt.range_, 999);
								setSlots (grants_, frame, 128, gnt.start_, gnt.range_, false);
								setSlots (service_, frame, 128, gnt.start_, gnt.range_, 9);
								setSlots (unconfirmedSlots_, frame, 128, gnt.start_, gnt.range_, false);
								setSlots (busy_, frame, 128, gnt.start_, gnt.range_, false);
								setSlots (self_rx_unavl_[ch], frame, 128, gnt.start_, gnt.range_, false);
								setSlots (neigh_tx_unavl_[ndx][ch], frame, 128, gnt.start_, gnt.range_, false);

								//if ( WimaxDebug::enabled() ) fprintf (stderr,
								//		"2sai do grantFit  gnt.range %d frame %d\n", gnt.range_, frame);
								return gnt;
							}
					}
					} else {
						if (serv_class == wimax::NRTPS || serv_class == wimax::RTPS) {

							if (serv_class == wimax::NRTPS && nSlots > nrtpsMinSlots_ ) nSlots = nrtpsMinSlots_;
							frame_room = false;

							for ( unsigned int r = 0 ; r < N ; r++ ) {

								// as soon as a free minislot is found, start the grant allocation
								if ( service_[F][r] == wimax::UGS) {

									gnt.service_ = serv_class;
									gnt.frame_ = frame;
									gnt.start_ = r;
									gnt.persistence_ = persistence;
									gnt.fromRequester_ = false;
									gnt.channel_ = ch;

									// search for the largest minislot range
									for ( ; r < N && service_[F][r] == wimax::UGS &&
										 ( r - gnt.start_ ) < nSlots ; r++ ) { }

									gnt.range_ = r - gnt.start_;
									if ( WimaxDebug::enabled() ) fprintf (stderr,
										"2gnt.range %d  %d-%d  frame %d\n", gnt.range_, gnt.start_, r, frame);

									unsigned int symbols =
										gnt.range_ * mac_->phyMib()->symPerSlot()
										- mac_->phyMib()->symShortPreamble();

									if ( symbols < minGrant_ ) continue;

									WimshMshDsch::AvlIE avl;
									avl.frame_ = gnt.frame_;
									avl.start_ = gnt.start_;
									avl.direction_ = WimshMshDsch::TX_AVL;
									avl.persistence_ = WimshMshDsch::FRAME128;
									avl.channel_ = ch;
									avl.service_ = serv_class;
									avl.range_ = gnt.range_;

									if ( dsch->remaining() > WimshMshDsch::AvlIE::size() + WimshMshDsch::GntIE::size() )
										dsch->add (avl);
									else {
										if (serv_class == wimax::NRTPS )
											availabilities_[0].push_back (avl);
										else
											availabilities_[1].push_back (avl);
										room = false;
									}

									setSlots (dst_, frame, 128, gnt.start_, gnt.range_, 999);
									setSlots (grants_, frame, 128, gnt.start_, gnt.range_, false);
									setSlots (service_, frame, 128, gnt.start_, gnt.range_, 9);
									setSlots (unconfirmedSlots_UGS_, frame, 128, gnt.start_, gnt.range_, false);
									setSlots (busy_UGS_, frame, 128, gnt.start_, gnt.range_, false);
									setSlots (self_rx_unavl_UGS_[ch], frame, 128, gnt.start_, gnt.range_, false);
									setSlots (neigh_tx_unavl_UGS_[ndx][ch], frame, 128, gnt.start_, gnt.range_, false);

									//if ( WimaxDebug::enabled() ) fprintf (stderr,
									//		"2sai do grantFit  gnt.range %d frame %d\n", gnt.range_, frame);
									return gnt;
								}
						}

					}
					}
				}
			}
		} // for each minislot

		// set the actual channel number
		ch = ( ch + 1 ) % C;

	} // for each channel
	//if ( WimaxDebug::enabled() ) fprintf (stderr, "!2frame totalmente ocupada %d\n", f);

	gnt.range_ = 0;
	if ( WimaxDebug::enabled() ) fprintf (stderr, "!3 final do grantfit \n");
	return gnt;
}

void
WimshBwManagerFairRR::realGrantStart (
		unsigned int ndx, unsigned int gframe, unsigned char gstart,
		unsigned char grange, unsigned char gchannel, WimshMshDsch::GntIE& gnt)
{
	unsigned int F;
	unsigned int c = 0;
	unsigned int s;
	std::bitset<MAX_SLOTS> map;



	for ( unsigned int f = gframe ; f < gframe + 10 ; f++ ) {
		F = f % HORIZON;

		map =
			unconfirmedSlots_[F] | unconfirmedSlots_UGS_[F] |
			busy_[F] | busy_UGS_[F] |
			self_rx_unavl_[gchannel][F] | self_rx_unavl_UGS_[gchannel][F] |
			neigh_tx_unavl_[ndx][gchannel][F] | neigh_tx_unavl_UGS_[ndx][gchannel][F];

		for ( s = gstart ; s < (gstart + grange) && map[s] == false ; s++ ) { }
		//if ( WimaxDebug::enabled() ) fprintf (stderr, "s %d\n",s);
		if ( WimaxDebug::trace("WBWM::realGrantStart") ) fprintf (stderr,
				"\trealGrantStart s %d\n", s);

		if ( s == (gstart + grange) ) {
			gnt.frame_ = gframe + c;
			//if ( WimaxDebug::enabled() ) fprintf (stderr, "realGrantStart gnt.frame_ %d\n",gnt.frame_);
			if ( WimaxDebug::trace("WBWM::realGrantStart") ) fprintf (stderr,
					"\trealGrantStart gnt.frame_ %d\n", gnt.frame_);
			return;
		} else
			c++;
	}
	if ( WimaxDebug::enabled() ) fprintf (stderr, "mais 10 gnt.frame_ %d\n",gnt.frame_);
	gnt.frame_ = gframe + 10;
}

void
WimshBwManagerFairRR::confFit (
		unsigned int f, unsigned int mstart,
		unsigned int mrange, WimshMshDsch::GntIE& gnt, bool& room,
		unsigned int serv_class, WimshMshDsch* dsch)
{
	WimshMshDsch::Persistence persistence;
	if ( serv_class == wimax::UGS ) persistence = WimshMshDsch::frames2pers(HORIZON);
	else persistence = WimshMshDsch::frames2pers(HORIZON / 4);

	unsigned int F = (f + 10) % HORIZON;

	std::bitset<MAX_SLOTS> map =
		busy_[F] | busy_UGS_[F] |
		self_tx_unavl_[gnt.channel_][F] | self_tx_unavl_UGS_[gnt.channel_][F];

	// for each minislot in the current frame
	for ( unsigned int s = mstart ; s < mstart + mrange ; s++ ) {

		// as soon as a free minislot is found, start the grant allocation
		if ( map[s] == false ) {

			//gnt.service_ = serv_class;
			gnt.frame_ = f;
			gnt.start_ = s;
			gnt.persistence_ = persistence;

			// search for the largest minislot range
			for ( ; s < ( mstart + mrange ) && map[s] == false ; s++ ) { }

			gnt.range_ = s - gnt.start_;
			return;
		}

		// grant range is full
		if ( s == (mstart + mrange - 1) && serv_class == wimax::NRTPS ) {
				if ( WimaxDebug::enabled() ) fprintf (stderr,
				"!1frame totalmente ocupada \n");
			for ( unsigned int s = mstart ; s < mstart + mrange ; s++ ) {

				// borrow bandwidth from nrtPS slots // TODO: review
				map[s] = ( service_[F][s] == wimax::NRTPS && ! busy_NRTPS_[F][s] &&
							! self_tx_unavl_NRTPS_[gnt.channel_][F][s] ) ? false : map[s];
			}

			for ( unsigned int s = mstart ; s < mstart + mrange ; s++ ) {

				// as soon as a free minislot is found, start the grant allocation
				if ( map[s] == false ) {

					//gnt.service_ = serv_class;
					gnt.frame_ = f;
					gnt.start_ = s;
					gnt.persistence_ = persistence;

					// search for the largest minislot range
					for ( ; s < ( mstart + mrange ) && map[s] == false ; s++ ) { }

					gnt.range_ = s - gnt.start_;

					if ( WimaxDebug::trace("WBWM::confFit") ) fprintf (stderr,
							"%.9f WBWM::confFit    [%d] grant range %d (%d-%d) frame %d\n",
							NOW, mac_->nodeId(), gnt.range_, gnt.start_, s, f);

					WimshMshDsch::AvlIE avl;
					avl.frame_ = gnt.frame_;
					avl.start_ = gnt.start_;
					avl.direction_ = WimshMshDsch::RX_AVL;
					avl.persistence_ = WimshMshDsch::FRAME32;
					avl.channel_ = gnt.channel_;
					avl.service_ = (serv_class == wimax::UGS ) ? wimax::RTPS : serv_class;
					avl.range_ = gnt.range_;

					if ( dsch->remaining() > WimshMshDsch::AvlIE::size() +
							WimshMshDsch::GntIE::size() )
						dsch->add (avl);
					else {
						availabilities_[0].push_back (avl);
						room = false;
					}
					return;
				}
			}
		}
	} // for each minislot

	// if we reach this point, then it is not possible to grant bandwidth
	gnt.range_ = 0;  // in this case, the other fields are not meaningful
}

void
WimshBwManagerFairRR::backlog (WimaxNodeId src, WimaxNodeId dst,
		unsigned char prio, WimaxNodeId nexthop, unsigned int bytes)
{
	// add the current flow to the weight manager data structure
	wm_.flow (src, dst, prio, mac_->neigh2ndx(nexthop), wimax::OUT);

	// get the index of the nexthop neighbor (ie. the link identifier)
	const unsigned int ndx = mac_->neigh2ndx(nexthop);

	// map high layer priority traffic to service class at mac layer
	unsigned char s = ( prio == 0 || prio == 1 ) ? 0 :
						( prio == 2 || prio == 3 ) ? 1 :
						( prio == 4 || prio == 5 ) ? 2 : 3 ;
	// add the amount of received bytes to the backlog of this output link
	neigh_[ndx][s].backlog_ += bytes;

	if ( WimaxDebug::trace("WBWM::backlog") ) fprintf (stderr,
			"%.9f WBWM::backlog    [%d] prio %d serv %d ndx %i\n", NOW, mac_->nodeId(), prio, s, ndx);
}

void
WimshBwManagerFairRR::backlog (WimaxNodeId nexthop, unsigned int bytes, unsigned int serv)
{
	// add the current flow to the weight manager data structure
	wm_.flow (mac_->neigh2ndx(nexthop), wimax::OUT);

	// get the index of the nexthop neighbor (ie. the link identifier)
	const unsigned int ndx = mac_->neigh2ndx(nexthop);

	// add the amount of received bytes to the backlog of this output link
	neigh_[ndx][serv].backlog_ += bytes;
}

void
WimshBwManagerFairRR::sent (WimaxNodeId nexthop, unsigned int bytes, unsigned int serv)
{
	// get the index of the nexthop neighbor (ie. the link identifier)
	const unsigned int ndx = mac_->neigh2ndx(nexthop);

	// remove the amount of received bytes from the backlog of this output link
	neigh_[ndx][serv].backlog_ -= bytes;
}

void
WimshBwManagerFairRR::searchTXslot (unsigned int ndx, unsigned int reqState)
{
	unsigned int fstart = mac_->frame(), // the current frame number
				 N = mac_->phyMib()->slotPerFrame(), // number of minislots per frame
				 dst = mac_->ndx2neigh (ndx), // neighbour ID (from the local identifier ndx)
				 flimit = 10, // number of frames to look in advance when trying to reserve slots
				 dschSize = WimshMshDsch::MAX_SIZE; // maximum size of the MSH-DSCH message (in bytes)

	// evaluate how many slots are required to transmit a DSCH (dschSize) to node ndx, according to the burst profile. No preamble. (?)
	unsigned int nslots = mac_->bytes2slots (ndx, dschSize, false);

	if ( WimaxDebug::trace("WBWM::searchTXslot") ) fprintf (stderr,
			"%.9f WBWM::srchTXslot [%d] dst %d reqState %d frame %d flimit %d nslots %d\n",
			NOW, mac_->nodeId(), dst, reqState, fstart, flimit, nslots);

	// Search for a slot in the next 'flimit' frames
	for ( unsigned int f = fstart + 1 ; f < fstart + flimit ; f++ ) {
		unsigned int F = f % HORIZON; // this frame's number mod horizon

		if ( WimaxDebug::trace("WBWM::searchTXslot") )
			fprintf (stderr, "\tFrame %d:\n", f);

		// obtain a bitset map of all slots unavailable for transmision
		const std::bitset<MAX_SLOTS> map =
				busy_[F] | busy_UGS_[F] | self_tx_unavl_[0][F] | self_tx_unavl_UGS_[0][F];

		// evaluate how many slots are already reserved for uncoord DSCHs
		unsigned markedForDSCH = 0;
		for(unsigned i = 0; i < N; i++)
			if ( uncoordsch_[F][i] != UINT_MAX )
				markedForDSCH++;

		if(markedForDSCH != 0)
			if ( WimaxDebug::trace("WBWM::searchTXslot") )
				fprintf (stderr, "\t\t%d slots already marked for uncoord DSCH\n", markedForDSCH);

		// check if there already are slots marked for uncoord DSCH to this node
		unsigned dschSlots = 0;
		for(unsigned i = 0; i < N; i++)
			if ( uncoordsch_[F][i] == dst )
				dschSlots++;

		if (dschSlots != 0)
			if ( WimaxDebug::trace("WBWM::searchTXslot") )
				fprintf (stderr, "\t\t%d slots already reserved to send to dst %d, needed %d\n", dschSlots, dst, nslots);
		// if we already have all the slots we need, leave
		if (dschSlots >= nslots) return;

		/*
		 * Try to find nslots starting at the end of the frame, where it's most likely to find free slots
		 * and not interfere with other allocations
		 */
		signed int s; 	// need signed to detect going beyond 0

		// locate the first free slot, going from finish to start
		for ( s = N-1 ; map[s] == true && (unsigned)s >= (nslots-1) ; s--);

		// if we didn't hit the start of the frame, try to find nslots free for us to use
		if( s != (signed)(nslots-2) ) {
			unsigned freeSlots=0;
			for( ; map[s] == false && s >= 0 && freeSlots < nslots ; s--)
				freeSlots++;

			// if we got what we needed, mark the slots for an uncoord DSCH and leave
			if( freeSlots == nslots ) {
				// aliases
				unsigned mstart = (unsigned)s;	// if we're here then s is >0
				unsigned &mrange = nslots;

				if ( WimaxDebug::trace("WBWM::searchTXslot") )
					fprintf (stderr, "\t\trange [%d:%d] is free, marking for uncoord DSCH to %d\n", mstart, mstart+mrange, dst);

				setSlots (uncoordsch_, 	f, 1, mstart, mrange, dst);
//				setSlots (grants_, 		f, 1, mstart, mrange, true);
				setSlots (busy_, 		f, 1, mstart, mrange, true);
				setSlots (self_tx_unavl_[0], f, 1, mstart, mrange, true);

				return;
			}
		}

		/*
		 * We were unable to find a contiguous range of free nslots.
		 * We now try to borrow slots from an existing rtPS reservation to dst.
		 * (but only if reqState==0, why?)
		 */
		if ( WimaxDebug::trace("WBWM::searchTXslot") )
			fprintf (stderr, "\t\tno free slots found, trying to borrow from rtPS reservations...\n");

		if ( reqState == 0 ) {
			// evaluate how many slots are reserved for rtPS in this frame
			unsigned int rtPSslots = 0;
			for ( unsigned int i = 0 ; i < N ; i++ )
				if ( dst_[F][i] == dst && service_[F][i] == wimax::RTPS )
					rtPSslots++;

			// if there are enough slots
			if( rtPSslots >= nslots) {
				// find a starting point, finish to start again
				signed int s;
				for( s = N-1 ; (dst_[F][s] != dst || service_[F][s] != wimax::RTPS) && s >=0; s--);

				// caution: this assumes the first reservation we find is size nslots
				unsigned mstart = s - nslots;
				unsigned &mrange = nslots;

				// use those slots
				setSlots (uncoordsch_, 	f, 1, mstart, mrange, dst);
//				setSlots (grants_, 		f, 1, mstart, mrange, true);
				setSlots (busy_, 		f, 1, mstart, mrange, true);
				setSlots (self_tx_unavl_[0], f, 1, mstart, mrange, true);

				if ( WimaxDebug::trace("WBWM::searchTXslot") )
					fprintf (stderr, "\t\trange [%d:%d] borrowed from rtPS reservations\n", mstart, mstart+mrange);

				return;
			}

			if ( WimaxDebug::trace("WBWM::searchTXslot") )
				fprintf (stderr, "\t\tunable to borrow slots from rtPS reservations\n");
		}

		if ( WimaxDebug::trace("WBWM::searchTXslot") )
			fprintf (stderr, "\t\tno slots found in rtPS, trying to borrow from other service classes\n");

		/*
		 * Here we try to borrow bandwidth previously allocated for other services, starting in Best Effort
		 * and up to UGS. Keep in mind that if multichannel is being used, then other nodes might be unable
		 * to receive messages in these slots if they're set to transmit in a channel other than zero.
		 */
		for ( unsigned int serv = wimax::BE ; serv < wimax::N_SERV_CLASS ; serv++ ) {
			// we already tried rtPS
			if ( serv == wimax::RTPS ) continue;

			// for each minislot in the current frame
			for ( unsigned int i = 0 ; i < N ; i++ ) {
				// find and mark slots to transmit MSH-DSCH message
				if ( 	grants_[F][i] == true &&
						service_[F][i] == serv &&
						uncoordsch_[F][i] != dst ) {

					// pick this slot number
					unsigned int mstart = i;

					// move 'i' forward up to 'nslots' while each slot is granted and assigned to the same service
					for ( ; i < (mstart + nslots) &&
							i < N &&
							grants_[F][i] == true &&
							service_[F][i] == serv &&
							uncoordsch_[F][i] != dst ; i++ );

					// get those nslots for uncoord DSCH
					if ( i == (mstart + nslots) ) {
						unsigned &mrange = nslots;

						setSlots (uncoordsch_, 	f, 1, mstart, mrange, dst);
		//				setSlots (grants_, 		f, 1, mstart, mrange, true);
						setSlots (busy_, 		f, 1, mstart, mrange, true);
						setSlots (self_tx_unavl_[0], f, 1, mstart, mrange, true);

						if ( WimaxDebug::trace("WBWM::searchTXslot") )
							fprintf (stderr, "\t\trange [%d:%d] borrowed from service %d\n", mstart, mstart+mrange, serv);

						return;
					}
				}
			}
		}

		if ( WimaxDebug::trace("WBWM::searchTXslot") ) fprintf (stderr,
				"\t\tUnable to borrow bandwidth from other service classes.\n");
	} // loop and look into the next frame if this frame had no slots available

	if ( WimaxDebug::trace("WBWM::searchTXslot") ) fprintf (stderr,
			"\t\tNo opportunities for uncoordinated DSCH found in frames [%d:%d]\n", fstart, fstart+flimit);

	// if sending in any of the next 'flimit' frames is impossible, mark send_rtps_together_
	// so it will be sent on the next normal (control subframe) MSH-DSCH opportunity
	send_rtps_together_[ndx] = true;
}

void
WimshBwManagerFairRR::printDataStructures (FILE* os)
{
	const char* class_names[4] = { "BE", "nrtPS", "rtPS", "UGS" };

	fprintf (os, "\tDATA STRUCTURES\n");
	for ( unsigned int s = 0 ; s < 4 ; s++ ) {
		fprintf (os, "\t%s\n", class_names[s] );
			for ( unsigned int i = 0 ; i < mac_->nneighs() ; i++ ) {
				fprintf (os, "\t%d in  %d req %d gnt %d cnf %d def %d\n",
						i, mac_->ndx2neigh(i),
						neigh_[i][s].req_in_, neigh_[i][s].gnt_in_, neigh_[i][s].cnf_in_,
						neigh_[i][s].def_in_);
				fprintf (os, "\t%d out %d req %d gnt %d cnf %d def %d backlog %d \n",
						i, mac_->ndx2neigh(i),
						neigh_[i][s].req_out_, neigh_[i][s].gnt_out_, neigh_[i][s].cnf_out_,
						neigh_[i][s].def_out_, neigh_[i][s].backlog_);
			}
	}
	fprintf (os, "\tREQUEST/GRANTING ACTIVE-LIST\n");
	for ( unsigned int s = 0 ; s < 4 ; s++ )
		WimaxDebug::print (activeList_[s], os, "\t");
}
