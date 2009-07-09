#include <wimsh_mossched.h>
#include <wimsh_scheduler_frr.h>
#include <stat.h>
#include <string>

#include <float.h>
#include <math.h>


static class WimshMOSSchedulerClass : public TclClass {
public:
	WimshMOSSchedulerClass() : TclClass("WimshMOSScheduler") {}
	TclObject* create(int, const char*const*) {
		return (new WimshMOSScheduler);
	}
} class_wimsh_mosscheduler;

WimshMOSScheduler::WimshMOSScheduler () : timer_(this)
{
	fprintf(stderr, "Initialized a MOS Scheduler\n");
//	timer_.resched(0.010);
}

int
WimshMOSScheduler::command(int argc, const char*const* argv)
{
	if ( argc == 3 && strcmp (argv[1], "mac") == 0 ) {
		mac_ = (WimshMac*) TclObject::lookup(argv[2]);
		return TCL_OK;
	} else if ( argc == 3 && strcmp (argv[1], "scheduler") == 0 ) {
		sched_ = (WimshSchedulerFairRR*) TclObject::lookup(argv[2]);
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[1], "enable") == 0 ) {
		enabled_ = TRUE;
		return TCL_OK;
	} else if ( argc == 2 && strcmp (argv[1], "disable") == 0 ) {
		enabled_ = FALSE;
		return TCL_OK;
	} else if ( argc == 3 && strcmp (argv[1], "maxcombs") == 0 ) {
		if ( atoi (argv[2]) <= 0 ) {
			fprintf (stderr, "Invalid number of max combinations '%d'. "
					"Choose a number greater than zero\n", atoi(argv[2]) );
			return TCL_ERROR;
		}
		maxCombs_ = (unsigned int) atoi (argv[2]);
		return TCL_OK;
	} else if ( argc == 3 && strcmp (argv[1], "ncombs") == 0 ) {
		if ( atoi (argv[2]) <= 0 ) {
			fprintf (stderr, "Invalid number of ncombs '%d'. "
					"Choose a number greater than zero\n", atoi(argv[2]) );
			return TCL_ERROR;
		}
		nCombs_ = (unsigned int) atoi (argv[2]);
		return TCL_OK;
	}
	return TCL_ERROR;
}

void
WimshMOSScheduler::statSDU(WimaxSdu* sdu)
{

//	fprintf(stderr, "\tDEBUG got sdu fid %d id %d\n", sdu->flowId(), sdu->seqnumber() );

	// the following code fills the stats_ vector with MOSFlowInfo elements, as new flowIDs are found
	{
		bool fid_present = FALSE;
		for (unsigned i=0; i < stats_.size(); i++) {
			// find this flowID in the vector, if it doesn't exist create it
			if(stats_[i].fid_ == sdu->flowId()) {
				fid_present = TRUE;
				break;
			}
		}
		if(!fid_present) {
			MOSFlowInfo flowstat (sdu->flowId(), -1); // this assumes the MOS scheduler always starts before the flows

			if(sdu->ip()->datalen()) {
				if(sdu->ip()->userdata()->type() == VOD_DATA) {
					flowstat.traffic_ = M_VOD;
				} else if(sdu->ip()->userdata()->type() == VOIP_DATA) {
					flowstat.traffic_ = M_VOIP;
				}
			} else {
				flowstat.traffic_ = M_FTP;
			}

			stats_.push_back(flowstat);
			fprintf(stderr, "%.9f WMOS::statSDU    [%d] Adding flow %d to stats\n",
					NOW, mac_->nodeId(), sdu->flowId());
		}
	}

	// now update the flow statistics

	// navigate to the position of MOSFlowInfo(FlowID)
	unsigned int n;
	for (n=0; n < stats_.size(); n++)
		{ if (stats_[n].fid_ == sdu->flowId()) break; }

	stats_[n].count_++;

//	fprintf(stderr, "\t\tstats_[n].lastuid_ %d sdu->seqnumber() %d\n", stats_[n].lastuid_, sdu->seqnumber());
 	if( sdu->nHops() != 0) // ignore the first node, no estimates yet
 	{
 		// if the packet uids received are not sequential, assume missing packets
		if( (stats_[n].lastuid_ + 1) != (int)sdu->seqnumber())
			stats_[n].lostcount_ += sdu->seqnumber() - (stats_[n].lastuid_ + 1);
		// update the last UID received
		stats_[n].lastuid_ = sdu->seqnumber();
 	}

	// update packet loss estimate
		// local estimation:
//			stats_[n].loss_ = (float)stats_[n].lostcount_ / (float)(stats_[n].lostcount_ + stats_[n].count_);
		// global estimation:
			stats_[n].loss_ = (float) Stat::get("e2e_owpl", sdu->flowId());

	// debug timestamps
//	fprintf(stderr, "\t DEBUG\n\t\tNOW %f\n\t\tTimestamp %f\n",
//			NOW, sdu->timestamp());

	// update delay estimate
		// local estimates
//			if( sdu->nHops() != 0) // ignore the first node, no estimates yet
//				stats_[n].delay_ = stats_[n].delay_*0.75 + (NOW - sdu->timestamp())*0.25;
		// global estimates
			stats_[n].delay_ = (float) Stat::get("e2e_owd_a", sdu->flowId());

	// update throughput estimate
		// global estimates
			stats_[n].tpt_ = (unsigned long) Stat::get("rd_ftp_tpt", sdu->flowId());

 	// debug delay
// 	fprintf(stderr, "\t DEBUG\n\t\tOld %f\n\t\tNew %f\n",
// 			stats_[n].delay_, NOW - sdu->timestamp() );


	// re-evaluate the flow's MOS
 	switch(stats_[n].traffic_) {
 	case M_VOIP:
// 		stats_[n].mos_ = audioMOS(stats_[n].delay_, stats_[n].loss_);
 		stats_[n].mos_ = updateMOS(M_VOIP, sdu->flowId());
 		break;

 	case M_VOD:
// 		stats_[n].mos_ = videoMOS( &(stats_[n].mse_), stats_[n].loss_ );
 		stats_[n].mos_ = updateMOS(M_VOD, sdu->flowId());
		break;

 	default:
 		stats_[n].mos_ = updateMOS(M_FTP, sdu->flowId());
 		break;
 	}

}

void
WimshMOSScheduler::dropPDU(WimaxPdu* pdu)
{
	// get the sdu
	WimaxSdu* sdu = pdu->sdu();

	/* we assume the first packet of a new flow is never dropped,
	 * so no checks are made to the stats_ vector
	 */

	// navigate to the position of MOSFlowInfo(FlowID)
	unsigned int n;
	for (n=0; n < stats_.size(); n++)
		{ if (stats_[n].fid_ == sdu->flowId()) break; }

	// increase the lost packet count
	stats_[n].lostcount_++;
	// update packet loss estimate
		// local estimation:
//			stats_[n].loss_ = (float)stats_[n].lostcount_ / (float)(stats_[n].lostcount_ + stats_[n].count_);
		// global estimation:
			stats_[n].loss_ = (float) Stat::get("e2e_owpl", sdu->flowId());

	// update throughput estimate
		// global estimates
			stats_[n].tpt_ = (unsigned long) Stat::get("rd_ftp_tpt", sdu->flowId());

	// distortion tracking and global statistics
	if(sdu->ip()->datalen()) {
		if(sdu->ip()->userdata()->type() == VOD_DATA) {
			VideoData* vodinfo_ = (VideoData*)sdu->ip()->userdata();
			// store MSE
			stats_[n].mse_.push_back(vodinfo_->distortion());
			// store ID
			stats_[n].vod_id_.push_back(sdu->seqnumber());

			// inform global statistics of the lost packet
			Stat::put ("rd_vod_lost_mse", sdu->flowId(), vodinfo_->distortion());
			Stat::put ("rd_vod_lost_frames", sdu->flowId(), 1);

		} else if(sdu->ip()->userdata()->type() == VOIP_DATA) {
			// VOIP lost frames
			// inform global statistics of the lost packet
			Stat::put ("rd_voip_lost_frames", sdu->flowId(), 1);
		}
	} else {
		// FTP lost frames
		Stat::put ("rd_ftp_lost_frames", sdu->flowId(), 1);
	}


	// re-evaluate the flow's MOS
 	switch(stats_[n].traffic_) {
 	case M_VOIP:
 		stats_[n].mos_ = updateMOS(M_VOIP, sdu->flowId());
// 		stats_[n].mos_ = audioMOS(stats_[n].delay_, stats_[n].loss_);
 		break;

 	case M_VOD:
// 		stats_[n].mos_ = videoMOS( &(stats_[n].mse_), stats_[n].loss_ );
 		stats_[n].mos_ = updateMOS(M_VOD, sdu->flowId());
 		break;

 	default:
 		stats_[n].mos_ = updateMOS(M_FTP, sdu->flowId());
 		break;
 	}

//	fprintf(stderr, "%.9f WMOS::dropPDU    [%d] Received PDU fid %d seq %d \n",
//			NOW, mac_->nodeId(), sdu->flowId(), sdu->seqnumber());

}

float
WimshMOSScheduler::audioMOS(double delay, float loss)
{
	/* data from:
	 * Improving Quality of VoIP Streams over WiMax
	 */

	float mos = 0;
	int R = 0;
	bool H = FALSE;
	float Id = 0, Ie = 0;

	// effects of delay
	delay *= 1000; // to ms
	delay += 25; // 25ms -> encoding time for G.711
	if( (delay-177.3) >= 0 ) H = TRUE;
	Id = 0.024*delay + 0.11*(delay - 177.3)*H;

	// effects of loss
	// for G.711
	int gamma1 = 0;
	int gamma2 = 30;
	int gamma3 = 15;
	Ie = gamma1 + gamma2*log(1+gamma3*loss);

	// R-factor
	R = 94.2 - Ie - Id;

	// MOS
	mos = 1 + 0.035*R + 0.000007*R*(R-60)*(100-R);

	// debug
//	fprintf(stderr, "\t[%d] audioMOS delay %f Id %f loss %f Ie %f mos %f\n",
//			mac_->nodeId(), delay, Id, loss, Ie, mos);

	return mos;

}

float
WimshMOSScheduler::dataMOS (float loss, unsigned long rate)
{
	/* data from:
	 * MOS-Based Multiuser Multiapplication Cross-Layer
	 * Optimization for Mobile Multimedia Communication
	 */

	// safeguards against rate zero
	if(rate==0) return 4.5;

	// a=2.1 & b=0.3 will fit the curve for MOS=1 at rate 10kbps and MOS 4.5 at rate 450kbps
	float data_a = 2.1;
	float data_b = 0.3;

	// expects rate in kbps
	float mos = data_a * log10(data_b*rate*(1-loss));

	// truncate the mos at maximum value
	if(mos>4.5) mos=4.5;

	return mos;
}

float
WimshMOSScheduler::videoMOS (vector<float>* mse, float loss)
{
	/* data from:
	 * Real-Time Monitoring of Video Quality in IP Networks
	 */

	// if no frames were lost, return MOS 4.5
	if(mse->size() == 0) {
//		fprintf(stderr, "\t[%d] videoMOS no lost frames, returning MOS 4.5\n", mac_->nodeId());
		return 4.5;
	}

	// define some parameters
	float gamma = 0; // attenuation factor
	float T = 10; // P-frames per GOP (10 in a 30-GOP)
	float alpha = ( pow(gamma, T+1) - (T+1)*gamma + T) / ( T*(1-gamma)*(1-gamma) ); // (5)


	// get sigma for (5), which is the average MSE of all dropped frames
	float mse_total = 0;
	for(unsigned i=0; i < mse->size(); i++)
	{
		mse_total += (*mse)[i];
	}

	float sigma = mse_total / mse->size();

	// obtain D1 (5)
	float D1 = alpha*sigma;

	// now obtain distortion D; we assume H.264 and D=s*n\*Pe*L*D1
//	float s = 8; 	// 8 slices per frame, CIF
	float s = 1; 	// since we have the MSE per frame, we consider 1 frame=1 slice
	float L = 1; 	// 1 frame per packet, encoding
	float n = 1;	// bernoulli, each loss affects 1 packet
	float Pe = loss;	// packet loss rate

	float D = s*n*Pe*L*D1;

	// map distortion D to PSNR
	float psnr = 10*log10(255*255/D);

	// map PSNR to quality
	float b1 = 0.5, b2 = 30;
	float ql = 1 / ( 1 + exp(b1*(psnr-b2)) );

	// map quality to MOS = Quality*(-3.5)+4.5
	float mos = ql * (-3.5) + 4.5;

//	fprintf(stderr, "MOS DEBUG psnr %f ql %f mos %f\n", psnr, ql, mos);

//	// linear mapping from PSNR 20dB (MOS 1) to 40dB (MOS 5)
//	float mos = psnr*0.20 - 3;

//	fprintf(stderr, "\t[%d] videoMOS:\n"
//			"\t\tgamma %f T %f alpha %f\n"
//			"\t\tlost %zd mse_total %f sigma %f D1 %f\n"
//			"\t\ts %f L %f n %f Pe %f\n"
//			"\t\tD %f PSNR %f MOS %f\n",
//			mac_->nodeId(),
//			gamma, T, alpha,
//			mse->size(), mse_total, sigma, D1,
//			s, L, n, Pe,
//			D, psnr, mos);

	return mos;
}

float
WimshMOSScheduler::mseVideoMOS (float mse, unsigned int nlost, float loss)
{
	if(mse== 0)
		return 4.5;

	// define some parameters
	float gamma = 0; // attenuation factor
	float T = 10; // P-frames per GOP (10 in a 30-GOP)
	float alpha = ( pow(gamma, T+1) - (T+1)*gamma + T) / ( T*(1-gamma)*(1-gamma) ); // (5)

	// get sigma for (5), which is the average MSE of all dropped frames
	float sigma = mse / nlost;

	// obtain D1 (5)
	float D1 = alpha*sigma;

	// now obtain distortion D; we assume H.264 and D=s*n\*Pe*L*D1
	//	float s = 8; 	// 8 slices per frame, CIF
	float s = 1; 	// since we have the MSE per frame, we consider 1 frame=1 slice
	float L = 1; 	// 1 frame per packet, encoding
	float n = 1;	// bernoulli, each loss affects 1 packet
	float Pe = loss;	// packet loss rate

	float D = s*n*Pe*L*D1;

	// map distortion D to PSNR
	float psnr = 10*log10(255*255/D);

	// map PSNR to quality
	float b1 = 0.5, b2 = 30;
	float ql = 1 / ( 1 + exp(b1*(psnr-b2)) );

	// map quality to MOS = Quality*(-3.5)+4.5
	float mos = ql * (-3.5) + 4.5;

//	fprintf(stderr, "MOS DEBUG psnr %f ql %f mos %f\n", psnr, ql, mos);

//	// linear mapping from PSNR 20dB (MOS 1) to 40dB (MOS 5)
//	float mos = psnr*0.20 - 3;

	return mos;
}


float
WimshMOSScheduler::deltaVideoMOS (vector<float>* mse, vector<float>* dropdist, MOSFlowInfo* flowinfo)
{
	float oldloss = flowinfo->loss_;
	float newloss = (float)(flowinfo->lostcount_ + dropdist->size()) /
					(float)(flowinfo->lostcount_ + dropdist->size() + flowinfo->count_);

	std::vector<float> totalmse;

	// push original
	for(unsigned i=0; i < mse->size(); i++)
		totalmse.push_back( (*mse)[i] );

	// combination drop increase
	for(unsigned i=0; i < dropdist->size(); i++)
		totalmse.push_back( (*dropdist)[i] );

	float oldMOS=videoMOS(mse, oldloss);
	float newMOS=videoMOS(&totalmse, newloss);

	assert( (newMOS - oldMOS) < 0 );
	return (newMOS - oldMOS);
}


void
WimshMOSScheduler::trigger(unsigned int target)
{
	if(WimaxDebug::trace("WMOS::buffMOS1"))
		fprintf(stderr, "%.9f WMOS::trigger    [%d] MOS Scheduler timer fired\n",
				NOW, mac_->nodeId());

	// run the buffer algorithms
	bufferMOS(target);

	// print some statistics, for debugging
	if(WimaxDebug::trace("WMOS::buffMOS1"))
	{
		fprintf (stderr,"\tflow statistics:\n");
		for (unsigned i=0; i < stats_.size(); i++) {
			fprintf (stderr,"\t\tfid %d lastuid %d count %d lost %d lossrate %f delay %f\n",
				stats_[i].fid_, stats_[i].lastuid_, stats_[i].count_, stats_[i].lostcount_, stats_[i].loss_,
				stats_[i].delay_);
		}
	}
}

void
WimshMOSScheduler::bufferMOS(unsigned int targetsize)
{
	// enabled_ is set via TCL and defines if the scheduler acts on the buffers
	if( !enabled_ )
		return;

	Stat::put("rd_scheduler_triggered", mac_->nodeId(), 1);

	// pointer to this node's scheduler
	WimshSchedulerFairRR* sched_ = (WimshSchedulerFairRR*)mac_->scheduler();


	// this scheduler won't work with per-flow or per-link buffer sharing
	if (sched_->BufferMode() == WimshSchedulerFairRR::PER_FLOW ||
			sched_->BufferMode() == WimshSchedulerFairRR::PER_LINK)
		{
			fprintf (stderr, "Warning: no buffer sharing, MOS scheduler will not function.\n");
			return;
		}

	// scheduler code enters here
	// Probably we need to do SHARED or PER_LINK buffer instead of PER_FLOW
	// WimaxPdu* pdu
	// desc.size_ -> buffer size
	// pdu->size()
	// link_[ndx][s] -> link queue
	// 	.rr_ -> Round robin list of flow descriptors
	// 	.queue_ -> Packet queue to go to the destination dst_ (std::queue<WimaxPdu*>)

	// a std::queue cannot be manipulated, so we either:
	//	- change all queues to vectors or lists
	//	- dump the queues, apply the algorithms, and then rebuild them

	// RDscheduler(WimaxPdu* pdu)

//	if(pdu->sdu()->ip()->datalen())
//		if(pdu->sdu()->ip()->userdata()->type() == VOD_DATA) {
//			VideoData* vodinfo_ = (VideoData*)pdu->sdu()->ip()->userdata();
//			fprintf (stderr, "\tGot a VOD_DATA packet, distortion %f\n", vodinfo_->distortion());
//		}

	if(WimaxDebug::trace("WMOS::buffMOS2"))
		fprintf (stderr, "\tPackets in the buffers:\n");

	// vector array to store all PDUs in the buffers
	// [ndx][serv][queueindex][pdu]
	std::vector< std::vector< std::vector< std::vector< WimaxPdu* > > > > pdulist_;
//	std::vector< std::vector< std::vector< WimaxPdu* > > > pdulist_;

	// resize the array
	pdulist_.resize(mac_->nneighs());
	for(unsigned i=0; i < mac_->nneighs(); i++)
		pdulist_[i].resize(wimax::N_SERV_CLASS);


	// pop all PDUs from the queues, to process
	// run all neighbors
	for(unsigned i=0; i < mac_->nneighs(); i++) {
		// run all services
		for(unsigned js=0; js < wimax::N_SERV_CLASS; js++) {
			// get the list of packet queues
			std::list<WimshSchedulerFairRR::FlowDesc> list_ = sched_->Link()[i][js].rr_.list();
			list<WimshSchedulerFairRR::FlowDesc>::iterator iter1 = list_.begin();

			// queue index, for indexing
			unsigned int qindex = 0;

			// resize the array for n packet queues
			unsigned lsize = list_.size();
			pdulist_[i][js].resize(lsize);

			// for each packet queue
			while( iter1 != list_.end() ) {
				// for each PDU
				while(!iter1->queue_.empty()) {
					// pop a PDU from the list into the vector
					WimaxPdu* temppdu = iter1->queue_.front();
					pdulist_[i][js][qindex].push_back(temppdu);
					iter1->queue_.pop();
				}
				// increment the queue index
				qindex++;
				// get the next packet queue
				iter1++;
			}
		}
	}


	if(WimaxDebug::trace("WMOS::buffMOS2"))
	{
		// show a list of all VOD_DATA packets in this node's buffers
		for(unsigned i=0; i < pdulist_.size(); i++)
			for(unsigned j=0; j < pdulist_[i].size(); j++)
				for(unsigned k=0; k < pdulist_[i].size(); k++)
					for(unsigned l=0; l < pdulist_[i][j][k].size(); l++) {
						if(pdulist_[i][j][k][l]->sdu()->ip()->datalen()) {
							if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOD_DATA) {
								VideoData* vodinfo_ = (VideoData*)pdulist_[i][j][k][l]->sdu()->ip()->userdata();
								fprintf (stderr, "\t\tVOD_DATA\tfid %d ndx %d id %d size %d\tdistortion %f\n",
										pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
										pdulist_[i][j][k][l]->size() ,vodinfo_->distortion());
							} else if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOIP_DATA) {
								fprintf (stderr, "\t\tVOIP_DATA\tfid %d ndx %d id %d size %d\n",
										pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
										pdulist_[i][j][k][l]->size());
							}
						} else {
								fprintf (stderr, "\t\tFTP_DATA\tfid %d ndx %d id %d size %d\n",
										pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
										pdulist_[i][j][k][l]->size());
						}
					}
	}

	// fill a vector with all flow ids of the packets in the buffers
	std::vector <int> flowids_;

	for(unsigned i=0; i < pdulist_.size(); i++)
		for(unsigned j=0; j < pdulist_[i].size(); j++)
			for(unsigned k=0; k < pdulist_[i].size(); k++)
				for(unsigned l=0; l < pdulist_[i][j][k].size(); l++) {
					// vector empty, push
					if(flowids_.size() == 0) {
						flowids_.push_back(pdulist_[i][j][k][l]->sdu()->flowId());
					} else {
						// see if the fid is already in the list
						bool inlist_ = false;
						for(unsigned int m=0; m < flowids_.size(); m++) {
							if(flowids_[m] == pdulist_[i][j][k][l]->sdu()->flowId()) {
								inlist_ = true;
							}
						}
						// not on the list, push
						if(!inlist_)
							flowids_.push_back(pdulist_[i][j][k][l]->sdu()->flowId());
					}
				}

	// print all flowIDs
	fprintf (stderr, "\tFlow IDs in the buffer: ");
	for(unsigned int i=0; i < flowids_.size(); i++)
		fprintf (stderr, "%d ", flowids_[i]);
	fprintf (stderr, " \n");

	// print flow ID information
	for(unsigned int i=0; i < stats_.size(); i++)
	{
		string traffic;
		switch(stats_[i].traffic_) {
		case M_VOD: traffic="VOD "; break;
		case M_VOIP: traffic="VOIP"; break;
		default: traffic="DATA";
		}
		fprintf (stderr, "\t\tfid %d traffic %s mos %f\n", stats_[i].fid_, traffic.c_str(), stats_[i].mos_);
	}

//	// see how full the buffer is
//	unsigned int buffusage = 0;
//	for(unsigned i=0; i < pdulist_.size(); i++)
//		for(unsigned j=0; j < pdulist_[i].size(); j++)
//			for(unsigned k=0; k < pdulist_[i].size(); k++)
//				for(unsigned l=0; l < pdulist_[i][j][k].size(); l++) {
//					buffusage += pdulist_[i][j][k][l]->size();
//				}
//	fprintf (stderr, "\tBuffer usage %d/%d, %.2f%%\n",
//			buffusage, sched_->maxBufSize(), ((float)buffusage/(float)sched_->maxBufSize())*100 );

	// see how full the buffer is
	fprintf (stderr, "\tBuffer usage %u/%u, %.2f%%\n",
			sched_->bufSize(), sched_->maxBufSize(),
			((float)sched_->bufSize()/(float)sched_->maxBufSize())*100 );

	// get the number of packets in the buffers
	unsigned int npackets = 0;
	for(unsigned i=0; i < pdulist_.size(); i++)
		for(unsigned j=0; j < pdulist_[i].size(); j++)
			for(unsigned k=0; k < pdulist_[i].size(); k++)
				npackets += pdulist_[i][j][k].size();

	fprintf (stderr, "\t%u packets in the buffers\n", npackets);

	// define scheduler aggressiveness here
//	float buffertrigger = 0.90;
	float buffertarget = 0.85;
	float reductionmargin = 0.01;
//	unsigned maxcombs = 5000;	// limit the maximum number of combinations to reduce processing time
	// maxCombs_ set via TCL

	if(npackets > 0 )
//			&& ((float)sched_->bufSize()/(float)sched_->maxBufSize()) > buffertrigger)
	{
		Stat::put ("rd_scheduler_triggered", 0, 1);
		fprintf (stderr, "\tRD Scheduler Triggered\n");

		/* here, we have:
		 * pdulist_[i][j][k][l] -> vector with all packets in the buffers
		 * npackets
		 * flowids_[i] -> vector of all flowIDs
		 * stats_[i] -> flowinfo stats
		 */

		// compute buffer decrease needs
		unsigned lbound=0, rbound=0;
		float targetpoint=0, bufferreduction=0;
		if(targetsize == 0)
		{
			// no target reduction given
			targetpoint = ( (float)sched_->maxBufSize() ) * buffertarget;
			bufferreduction = (float)sched_->bufSize() - targetpoint;
			lbound = (unsigned)( bufferreduction - (float)sched_->maxBufSize()*reductionmargin );
			rbound = (unsigned)( bufferreduction + (float)sched_->maxBufSize()*reductionmargin );
		}
		else
		{
			// target reduction given
			targetpoint = ( (float)sched_->maxBufSize() ) * buffertarget;
			bufferreduction = (float)sched_->bufSize() - targetpoint;

			if((unsigned)bufferreduction < targetsize)
			{
				// if the incoming packet needs more space than what we're trying to get
				bufferreduction = (float)targetsize;
				lbound = (unsigned)( bufferreduction );
				rbound = (unsigned)( bufferreduction + (float)sched_->maxBufSize()*2*reductionmargin );
				fprintf(stderr, "\t\tOverriding buffer reduction target due to oversized packet\n");
			}
			else
			{
				// proceed normally
				lbound = (unsigned)( bufferreduction - (float)sched_->maxBufSize()*reductionmargin );
				rbound = (unsigned)( bufferreduction + (float)sched_->maxBufSize()*reductionmargin );
			}

		}

//		fprintf(stderr, "\tDEBUG BUFF tgtpoint %f bfreduct %f lbound %u rbound %u\n",
//				targetpoint, bufferreduction, lbound, rbound);

		// need to get all packet combinations that reduce the buffer by X
		// combinations represent binary values as to whether the packet has been chosen or not
		// combinations (2^npackets)

		long int ncombs = 0;
		if( npackets > nCombs_ )	// 2^nCombs_ set via TCL
			ncombs = pow(2, nCombs_);
		else
			ncombs = pow(2, npackets);

		fprintf (stderr, "\tevaluating %ld combinations for size match\n", ncombs);

		if(WimaxDebug::trace("WMOS::buffMOS2"))
			fprintf (stderr, "\t\tcombination matches for [%u,%u]:\n\t\tID:", lbound, rbound);
//		std::vector<bool> binComb(npackets, 0);
		std::vector<long> validCombs;
		for (long combID = 0; combID < ncombs && validCombs.size() < maxCombs_; combID++)
		{
			std::vector<bool> binComb;
			dec2bin(combID, &binComb);

			// fill the binComb for missing zeroes
			for(unsigned k=binComb.size(); k<npackets; k++)
				binComb.push_back(0);

			unsigned int combsize = 0;
			unsigned int packetid = 0;
			for(unsigned i=0; i < pdulist_.size(); i++)
				for(unsigned j=0; j < pdulist_[i].size(); j++)
					for(unsigned k=0; k < pdulist_[i].size(); k++)
						for(unsigned l=0; l < pdulist_[i][j][k].size(); l++)
						{
							if(combsize > rbound) break; // premature break in order to speed up the process
							if(binComb[packetid] == TRUE)
								combsize += pdulist_[i][j][k][l]->size();
							packetid++;
						}

			if(combsize > lbound && combsize < rbound) {
				validCombs.push_back(combID);
				if(WimaxDebug::trace("WMOS::buffMOS2"))
					fprintf (stderr, " %ld", combID);
			}
		}
		fprintf (stderr, "\n");


		if( validCombs.size() == 0)
			fprintf (stderr, "\t0 combinations matched, aborting...\n");
		else
		{
			// here we have: validCombs, with all matching combinations
			// use dec2bin(combID, &binComb) to get packet references
			fprintf (stderr, "\t%zd combinations matched, processing...\n", validCombs.size());

			// process all combinations
			// vector to hold CombInfo elements
			std::vector<CombInfo> combstats_;
			for(unsigned p=0; p<validCombs.size(); p++)
			{
				if(WimaxDebug::trace("WMOS::buffMOS2"))
					fprintf (stderr, "\t\tcombID %ld:\n", validCombs[p]);

				std::vector<bool> binComb;
				dec2bin(validCombs[p], &binComb);
				for(unsigned k=binComb.size(); k<npackets; k++)
					binComb.push_back(0);
				unsigned int packetid = 0;
				std::vector<WimaxPdu*> combPdu;
				for(unsigned i=0; i < pdulist_.size(); i++)
					for(unsigned j=0; j < pdulist_[i].size(); j++)
						for(unsigned k=0; k < pdulist_[i].size(); k++)
							for(unsigned l=0; l < pdulist_[i][j][k].size(); l++)
							{
								if(binComb[packetid] == TRUE)
								{
									if(WimaxDebug::trace("WMOS::buffMOS3"))
									{
									// print the packet info
										if(pdulist_[i][j][k][l]->sdu()->ip()->datalen()) {
											if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOD_DATA) {
												VideoData* vodinfo_ = (VideoData*)pdulist_[i][j][k][l]->sdu()->ip()->userdata();
												fprintf (stderr, "\t\t\tVOD_DATA\tfid %d ndx %d id %d size %d\tdistortion %f\n",
														pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
														pdulist_[i][j][k][l]->size() ,vodinfo_->distortion());
											} else if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOIP_DATA) {
												fprintf (stderr, "\t\t\tVOIP_DATA\tfid %d ndx %d id %d size %d\n",
														pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
														pdulist_[i][j][k][l]->size());
											}
										} else {
												fprintf (stderr, "\t\t\tFTP_DATA\tfid %d ndx %d id %d size %d\n",
														pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
														pdulist_[i][j][k][l]->size());
										}
									}
								// store the packet for MOS processing
									combPdu.push_back(pdulist_[i][j][k][l]);
								}
								packetid++;
							}


				// calculate MOS and impact here
				// vector combPdu is where it's at
				{
					// fill a vector with the flowIDs in this packet combination
					std::vector <int> combfIDs;
					for(unsigned l=0; l < combPdu.size(); l++)
					{
						// vector empty, push
						if(combfIDs.size() == 0) {
							combfIDs.push_back(combPdu[l]->sdu()->flowId());
						} else {
							// see if the fid is already in the list
							bool inlist_ = false;
							for(unsigned int m=0; m < combfIDs.size(); m++) {
								if(combfIDs[m] == combPdu[l]->sdu()->flowId()) {
									inlist_ = true;
								}
							}
							// not on the list, push
							if(!inlist_)
								combfIDs.push_back(combPdu[l]->sdu()->flowId());
						}
					}

					// for each flow, aggregate all of its packets and estimate MOS impact

					float mosweight = -0.05; // greater impact to flows w/ good MOS
					float vodweight = 1; 	// coefficient specific to VOD flows
					float voipweight = 1;	// coefficient specific to VOIP flows
					float ftpweight = 1; 	// coefficient specific to FTP flows

					vector<float> combMOSdrop; // stores accumulated MOS variations for this combID
					for(unsigned l=0; l < combfIDs.size(); l++)
					{
						// get the FlowInfo of this flowID
						unsigned k;
						for(k=0; stats_[k].fid_ != combfIDs[l]; k++);

						if(stats_[k].traffic_ == M_VOD)
						{
							// VOD, sum the distortion of all packets and pass it to deltaVideoMOS
							vector<float> dropdist; unsigned nCombPackets=0;
							for(unsigned n=0; n<combPdu.size(); n++) // for each PDU
							{
								if(combPdu[n]->sdu()->flowId() == combfIDs[l]) // if it belongs to the current flowID
								{
									if(combPdu[n]->sdu()->ip()->userdata()->type() == VOD_DATA) { // and is VOD
										VideoData* vodinfo_ = (VideoData*)combPdu[n]->sdu()->ip()->userdata();
										dropdist.push_back(vodinfo_->distortion()); // accumulate distortion
										nCombPackets++;
									}
								}
							}

							// {fid, dist, packets}: {combfIDs[l], totalDist, nCombPackets}
							assert(nCombPackets>0);

							// nCombPackets, estimate drop percentage increase, get new MOS
							float deltaMOS = deltaVideoMOS(&(stats_[k].mse_), &dropdist, &(stats_[k]));

							// apply a weight to this deltaMOS, based on the flow's MOS
							{
								// get the FlowInfo
								unsigned r=0;
								for(; r<stats_.size(); r++)
									if(stats_[r].fid_ == combfIDs[l])
										break;
								// get the MOS
								float flowMOS = stats_[r].mos_;

								// apply the weights
								deltaMOS += (mosweight*vodweight*flowMOS); // mosweight should be negative
							}

							// associate the deltaMOS to this combID for later processing
							combMOSdrop.push_back(deltaMOS);

							if(WimaxDebug::trace("WMOS::buffMOS2"))
								fprintf(stderr, "\t\t\t(-) VOD  fid %d drop %u delta %f\n",
										combfIDs[l], nCombPackets, deltaMOS);
						}
						else if(stats_[k].traffic_ == M_VOIP)
						{
							// VOIP, get the number of packets for error percentage
							unsigned nCombPackets=0;
							for(unsigned n=0; n<combPdu.size(); n++) // for each PDU
								if(combPdu[n]->sdu()->flowId() == combfIDs[l]) // if it belongs to the current flowID
									nCombPackets++;

							assert(nCombPackets>0);

							// implicit global stats:
								// nCombPackets, estimate drop percentage increase, get new MOS
								float newloss = (float)(stats_[k].lostcount_ + nCombPackets) /
												(float)(stats_[k].lostcount_ + nCombPackets + stats_[k].count_);

								float newMOS = audioMOS(stats_[k].delay_, newloss);
								float deltaMOS = newMOS - stats_[k].mos_;

							// apply a weight to this deltaMOS, based on the flow's MOS
							{
								// get the FlowInfo
								unsigned r=0;
								for(; r<stats_.size(); r++)
									if(stats_[r].fid_ == combfIDs[l])
										break;
								// get the MOS
								float flowMOS = stats_[r].mos_;

								// apply the weight
								deltaMOS += (mosweight*flowMOS); // mosweight should be negative
								// apply the global VOIP weight
								deltaMOS *= voipweight;
							}

							// associate the deltaMOS to this combID for later processing
							combMOSdrop.push_back(deltaMOS);

							if(WimaxDebug::trace("WMOS::buffMOS2"))
								fprintf(stderr, "\t\t\t(-) VOIP fid %d drop %d delta %f oldMOS %f newMOS %f\n",
										combfIDs[l], nCombPackets, deltaMOS, stats_[k].mos_, newMOS);

						}
						else
						{
							// FTP, need packet loss and tpt
							unsigned nCombPackets=0, combpsize=0;
							for(unsigned n=0; n<combPdu.size(); n++) // for each PDU
								if(combPdu[n]->sdu()->flowId() == combfIDs[l]) // if it belongs to the current flowID
								{
									nCombPackets++;
									// get total size of packets
									combpsize += combPdu[n]->size();
								}

							assert(nCombPackets>0);

							// implicit global stats:
								// nCombPackets, estimate drop percentage increase, get new MOS
								float newloss = (float)(stats_[k].lostcount_ + nCombPackets) /
												(float)(stats_[k].lostcount_ + nCombPackets + stats_[k].count_);


							// get old tpt
							float oldtpt = stats_[k].tpt_; // in B/s
							oldtpt *= 8; // to bps

							// reduction factor due to packet drop
							// needs much improvement here
							float newtpt = oldtpt - combpsize;
							if(newtpt<0) newtpt=0; // safety check

							newtpt /= 1000; // to kbps

							float newMOS = dataMOS(newloss, newtpt);
							float deltaMOS = newMOS - stats_[k].mos_;

							// apply a weight to this deltaMOS, based on the flow's MOS
							{
								// get the FlowInfo
								unsigned r=0;
								for(; r<stats_.size(); r++)
									if(stats_[r].fid_ == combfIDs[l])
										break;
								// get the MOS
								float flowMOS = stats_[r].mos_;

								// apply the weight
								deltaMOS += (mosweight*flowMOS); // mosweight should be negative
								// apply the global VOIP weight
								deltaMOS *= ftpweight;
							}

							// associate the deltaMOS to this combID for later processing
							combMOSdrop.push_back(deltaMOS);

							if(WimaxDebug::trace("WMOS::buffMOS2"))
								fprintf(stderr, "\t\t\t(-) FTP  fid %d drop %d delta %f oldMOS %f newMOS %f\n",
										combfIDs[l], nCombPackets, deltaMOS, stats_[k].mos_, newMOS);

						}

					} // end of for each fID of this comb


					// process list of distortion impacts, combMOSdrop[]
					{
						// total drop
						float totalMOSdrop = 0;
						for(unsigned i=0; i<combMOSdrop.size(); i++)
							totalMOSdrop+=combMOSdrop[i];

						// average drop
						float avgMOSdrop = totalMOSdrop / combMOSdrop.size();

						// standard deviation
						float stdMOSdrop = stddev(&combMOSdrop);

						// store info on vector combstats_
						CombInfo tempcombinfo(validCombs[p], totalMOSdrop, avgMOSdrop, stdMOSdrop);
						combstats_.push_back(tempcombinfo);

						if(WimaxDebug::trace("WMOS::buffMOS2"))
							fprintf(stderr, "\t\t\t(>) combID %ld flows impacted %zd MOSimpact total %f avg %f std %f\n",
									validCombs[p], combMOSdrop.size(), totalMOSdrop, avgMOSdrop, stdMOSdrop);
					}

				} // end of MOS calculations

			} // end of combination packet listing

			// algorithm to choose the best combID from combstats_
			long dropCombID = 0; // best combination for dropping
			float combImpact = -500; // valor

			float stddevcoeff = 1;

			for(unsigned r=0; r < combstats_.size(); r++)
			{
				float impact = combstats_[r].total_ - stddevcoeff*combstats_[r].std_;
				combstats_[r].impact_ = impact;

	//			fprintf(stderr, "\tDEBUG impact %f total %f std %f\n",
	//					impact, combstats_[r].total_, stddevcoeff*combstats_[r].std_);

				if(impact > combImpact) // we're working with negative values
				{
					combImpact = impact;
					dropCombID = combstats_[r].combID_;
				}

			}

			// sum up results
			if(WimaxDebug::trace("WMOS::buffMOS1"))
			{
				fprintf(stderr, "\tsynopsis of combinations:\n");
				for(unsigned j=0; j<combstats_.size(); j++)
				{
					fprintf(stderr, "\t\tcombID %ld:\t total %f avg %f std %f impact %f\n",
							combstats_[j].combID_, combstats_[j].total_, combstats_[j].avg_, combstats_[j].std_,
							combstats_[j].impact_);
				}
			}

			if(WimaxDebug::trace("WMOS::buffMOS0"))
				fprintf(stderr, "\tSelecting combID %ld for drop, impact %f\n", dropCombID, combImpact);

			// packet dropping of chosen combID
			std::vector<bool> binComb;
			dec2bin(dropCombID, &binComb);

			// fill the binComb for missing zeroes
			for(unsigned k=binComb.size(); k<npackets; k++)
				binComb.push_back(0);

			// kill the packets
			unsigned int packetid = 0;
			for(unsigned i=0; i < pdulist_.size(); i++)
				for(unsigned j=0; j < pdulist_[i].size(); j++)
					for(unsigned k=0; k < pdulist_[i].size(); k++)
					{
						vector<WimaxPdu*>::iterator iter1 = pdulist_[i][j][k].begin();
						while( iter1 != pdulist_[i][j][k].end())
						{
							if(binComb[packetid] == TRUE)
							{
								WimaxPdu* pdu = *iter1;
								fprintf (stderr, "\t\tDropping packet fid %d ndx %d id %d size %d type %d\n",
										pdu->sdu()->flowId(), i, pdu->sdu()->seqnumber(),
										pdu->size(), pdu->sdu()->ip()->datalen()?pdu->sdu()->ip()->userdata()->type():0);

								// stat the lost packet as killed by the scheduler
								Stat::put ("rd_packet_lost_scheduler", pdu->sdu()->flowId(), 1);

								dropPDU(pdu);
	//							pdu->sdu()->freePayload();
	//							delete pdu->sdu();
	//							delete pdu;

								// reduce buffer occupancy
								// TODO: this is not the way to kill the packets
								sched_->setBufSize(sched_->bufSize() - pdu->size());

								// erase packet from the pdu list
								pdulist_[i][j][k].erase(iter1);
							} else
							{ ++iter1; } // iter1 is autoincremented after erase()
							packetid++;
						 }
					}

		} // end of buffer size check

		// re-print the packet list, for tests
		fprintf (stderr, "\tResulting buffer:\n");

		// see how full the buffer is
		fprintf (stderr, "\t\tBuffer usage %u/%u, %.2f%%\n",
				sched_->bufSize(), sched_->maxBufSize(),
				((float)sched_->bufSize()/(float)sched_->maxBufSize())*100 );

		if(WimaxDebug::trace("WMOS::buffMOS2"))
		{
			for(unsigned i=0; i < pdulist_.size(); i++)
				for(unsigned j=0; j < pdulist_[i].size(); j++)
					for(unsigned k=0; k < pdulist_[i].size(); k++)
						for(unsigned l=0; l < pdulist_[i][j][k].size(); l++) {
							if(pdulist_[i][j][k][l]->sdu()->ip()->datalen()) {
								if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOD_DATA) {
									VideoData* vodinfo_ = (VideoData*)pdulist_[i][j][k][l]->sdu()->ip()->userdata();
									fprintf (stderr, "\t\tVOD_DATA\tfid %d ndx %d id %d size %d\tdistortion %f\n",
											pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
											pdulist_[i][j][k][l]->size() ,vodinfo_->distortion());
								} else if(pdulist_[i][j][k][l]->sdu()->ip()->userdata()->type() == VOIP_DATA) {
									fprintf (stderr, "\t\tVOIP_DATA\tfid %d ndx %d id %d size %d\n",
											pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
											pdulist_[i][j][k][l]->size());
								}
							} else {
									fprintf (stderr, "\t\tFTP_DATA\tfid %d ndx %d id %d size %d\n",
											pdulist_[i][j][k][l]->sdu()->flowId(), i, pdulist_[i][j][k][l]->sdu()->seqnumber(),
											pdulist_[i][j][k][l]->size());
							}
						}
		}
	}

	// reconstruct queues
	for(unsigned i=0; i < mac_->nneighs(); i++) {
		// run all services
		for(unsigned js=0; js < wimax::N_SERV_CLASS; js++) {
			std::list<WimshSchedulerFairRR::FlowDesc> list_ = sched_->Link()[i][js].rr_.list();
			list<WimshSchedulerFairRR::FlowDesc>::iterator iter1 = list_.begin();

			// queue index, for indexing
			unsigned int qindex = 0;

			// for each packet queue
			while( iter1 != list_.end() ) {
				// for each PDU
				for(unsigned k=0; k < pdulist_[i][js][qindex].size(); k++)
					iter1->queue_.push(pdulist_[i][js][qindex][k]);
				iter1++;
			}

		}
	}
}

float
WimshMOSScheduler::updateMOS(MOStraffic traffic, int flowID)
{
	if(traffic==M_VOD)
	{
		// get the number of lost frames
		unsigned int nlost = (unsigned int) Stat::get("rd_vod_lost_frames", flowID);
		// get the cumulative mse lost
		float mselost = (float) Stat::get("rd_vod_lost_mse", flowID);
		// calculate the loss
		float loss = (float)nlost / (float)(Stat::get("rd_vod_recv_frames", flowID) + nlost);

		// get the new MOS
		float mos = mseVideoMOS(mselost, nlost, loss);

		// update MOS stat
		Stat::put ("rd_vod_mos", flowID, mos);

		return mos;
	}
	else if(traffic==M_VOIP)
	{
		// get the flow's error rate
		float ploss = (float) Stat::get("e2e_owpl", flowID);
		// get the delay
		float delay = (float) Stat::get("e2e_owd_a", flowID);

		float mos = audioMOS(delay, ploss);

		// update MOS stat
		Stat::put ("rd_voip_mos", flowID, mos);

		return mos;
	}
	else
	{	// FTP
		// get the flow's error rate
		float ploss = (float) Stat::get("e2e_owpl", flowID);
		// get the flow's throughput
			// tpt in Stat:: "e2e_tpt" is only obtained when simulation finishes
			// unsigned long tpt = (unsigned long) Stat::get("e2e_tpt", flowID);
		unsigned long tpt = (unsigned long) Stat::get("rd_ftp_tpt", flowID);

		// convert to kbps
		tpt *= 8;
		tpt /= 1000;

		// calculate the MOS
		float mos = dataMOS(ploss, tpt);

		// update MOS stat
		Stat::put ("rd_ftp_mos", flowID, mos);

		return mos;
	}

	return 0;
}

void
MOStimer::expire(Event *e) {
	// call the handle function for the local node
	a_->trigger();

	// reschedule
	a_->gettimer().resched(0.010); // a_->interval_ (and define via TCL)
	// enable/disable in WimshMOSScheduler::WimshMOSScheduler ()
}

void
WimshMOSScheduler::dec2bin(long decimal, vector<bool>* binary)
{
	long remain;
	vector<bool> temp;
	do	{
		remain = decimal % 2;
		// whittle down the decimal number
		decimal = decimal / 2;
		// converts digit 0 or 1 to character '0' or '1'
		binary->push_back(remain);
	} while (decimal > 0);
}

float
WimshMOSScheduler::stddev(vector<float>* values)
{
	float total = 0, mean = 0, stddev = 0;

	// get the total
	for(unsigned i=0; i < values->size(); i++)
		total += (*values)[i];

	// get the mean
	mean = total / values->size();

	// get the squares
	float sqraccum = 0;
	for(unsigned i=0; i < values->size(); i++)
		sqraccum += pow( ((*values)[i] - mean), 2);

	// get the avg square
	float avgsqr = sqraccum / values->size();

	// get the stddev
	stddev = sqrt(avgsqr);

	return stddev;
}
