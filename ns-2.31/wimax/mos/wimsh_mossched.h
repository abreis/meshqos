

#ifndef __NS2_WIMSH_MOS_SCHEDULER_H
#define __NS2_WIMSH_MOS_SCHEDULER_H

#include <wimax_common.h>
#include <timer-handler.h>

#include <wimsh_mac.h>

#include <videodata.h>
#include <ns-process.h>

#include <vector>

class WimshMac;
class WimshMOSScheduler;
class WimshSchedulerFairRR;

enum MOStraffic { M_VOD, M_VOIP, M_FTP, M_NTRAFFIC };

struct MOSFlowInfo {
	//! Flow ID
	int fid_;
	//! Flow type
	MOStraffic traffic_;
	//! Last packet UID received
	int lastuid_;
	//! Packet count
	unsigned int count_;
	//! Lost packet count
	unsigned int lostcount_;
	//! Packet loss estimate
	float loss_;

	//! Delay information
	double delay_;

	//! Throughput information (for FTP)
	unsigned long tpt_;

	//! MOS of the flow
	float mos_;

	//! distortion information
	// list of lost video packet MSEs
	vector<float> mse_;
	// list of lost video packet IDs
	vector<int> vod_id_;


	//! Constructor
	MOSFlowInfo (int fid = 0, int uid = 0) {
		fid_ = fid; count_ = 0;
		lostcount_ = 0; lastuid_ = uid; loss_ = 0;
		delay_ = 0; tpt_ = 0; mos_ = 0;
	}
};

// structure to hold {combID, totalDrop, avgDrop, stdDrop}
struct CombInfo {
	long combID_;
	float total_, avg_, std_, impact_;
	CombInfo(long combID, float total, float avg, float std, float impact = 0) {
		combID_ = combID; total_ = total; avg_ = avg; std_ = std; impact_ = impact;
	}
};

class MOStimer : public TimerHandler {
public:
	MOStimer(WimshMOSScheduler *a) : TimerHandler() { a_ = a; }
protected:
	virtual void expire(Event *e);
	WimshMOSScheduler *a_;
};

class WimshMOSScheduler : public TclObject {
public:
	//! Do nothing.
	WimshMOSScheduler ();
	//! Do nothing.
	virtual ~WimshMOSScheduler () { }

	//! Called each time the timer fires
	void trigger(unsigned int target=0);
	//! Return the timer object
	MOStimer& gettimer() { return timer_; }
	//! Handle the timer event
	void handle ();

	//! Apply the scheduler algoritm to the buffers
	void bufferMOS(unsigned int targetsize=0);

	//! Process an SDU for statistics
	void statSDU(WimaxSdu* sdu);
	//! Process a dropped PDU for statistics
	void dropPDU(WimaxPdu* pdu);

	//! Obtain MOS for an audio flow
	float audioMOS (double delay, float loss);
	//! Obtain MOS for a video flow
	float videoMOS (vector<float>* mse, float loss);
	//! Obtain MOS for a video flow, given only the total lost MSE
	float mseVideoMOS (float mse, unsigned int nlost, float loss);
	//! Obtain deltaMOS for a video flow
	float deltaVideoMOS (vector<float>* mse, vector<float>* dropdist, MOSFlowInfo* flowinfo);
	//! Obtain MOS for a data flow
	float dataMOS (float loss, unsigned long rate);

	//! Update the global MOS of a flow
	float updateMOS(MOStraffic traffic, int flowID);

	//! Vector of MOSFlowInfo structs to keep track of data
	std::vector <MOSFlowInfo> stats_;
private:

protected:
	//! Tcl interface.
	virtual int command(int argc, const char*const* argv);

	//! Parameters for data MOS calculation. Set via TCL.
//	float data_a=0, data_b=0;

	// enable or disable the MOS scheduler. Set via TCL.
	bool enabled_;

	//! Trigger timer
	MOStimer timer_;

	//! Pointer to the MAC layer.
	WimshMac* mac_;
	//! Pointer to the Scheduler
	WimshSchedulerFairRR* sched_;

	//! Maximum number of packet combinations to evaluate for MOS
	unsigned int maxCombs_;
	//! Maximum number of packet combinations to evaluate for size (power of 2, i.e. 2^ncombs)
	unsigned int nCombs_;


	//! convert a long int to a binary value, stored in a vector<bool>
	void dec2bin(long decimal, vector<bool>* binary);
	//! compute the standard deviation of an array of floats
	float stddev(vector<float>* values);
};



#endif __NS2_WIMSH_MOS_SCHEDULER_H
