/* Copyright (c) 2009 Andre Braga Reis
 * andrebragareis at gmail dot com
 *
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linking this file statically or dynamically with other modules is making
 * a combined work based on this file.  Thus, the terms and conditions of
 * the GNU General Public License cover the whole combination.
 *
 * In addition, as a special exception, the copyright holders of this file
 * give you permission to combine this file with free software programs or
 * libraries that are released under the GNU LGPL and with code included in
 * the standard release of ns-2 under the Apache 2.0 license or under
 * otherwise-compatible licenses with advertising requirements (or modified
 * versions of such code, with unchanged license).  You may copy and
 * distribute such a system following the terms of the GNU GPL for this
 * file and the licenses of the other code concerned, provided that you
 * include the source code of that other code when and as the GNU GPL
 * requires distribution of source code.
 *
 * Note that people who make modified versions of this file are not
 * obligated to grant this special exception for their modified versions;
 * it is their choice whether to do so.  The GNU General Public License
 * gives permission to release a modified version without this exception;
 * this exception also makes it possible to release a modified version
 * which carries forward this exception.
 */

/*
 * description
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "config.h"
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#include "random.h"
#include "object.h"
#include "trafgen.h"

#include "videodata.h"

/* TODO: deal with errors
 */

struct tracerec {
	u_int32_t trec_time; // inter-packet time (usec)
	u_int32_t trec_size; // size of packet (bytes)
	u_int32_t trec_dist; // distortion importance

//	u_int32_t trec_ftype;	// frame type {I,P,B} (char)
//							// due to the lack of an htonX() for uint8, we 'waste' 24-bits
};

// object to hold a single trace file
class VODTraceFile : public NsObject {
 public:
	VODTraceFile();
	void get_next(int&, struct tracerec&); // called by TrafficGenerator to get next record in trace
	virtual int setup();  // initialize the trace file
	int command(int argc, const char*const* argv);
 protected:
	void recv(Packet*, Handler*); // must be defined for NsObject
	int status_;	//
	char *name_;  // name of the file in which the trace is stored
	int nrec_;    // number of records in the trace file
	struct tracerec *trace_; // array holding the trace
};

/* Instance of a traffic generator. Has a pointer to the VODTraceFile
 * object and implements the interval() function.
 */

class VODTrafficTrace : public TrafficGenerator {
 public:
	VODTrafficTrace();
	int command(int argc, const char*const* argv);
	virtual double next_interval(int &);
 protected:
	virtual void timeout();
	VODTraceFile *tfile_;
	struct tracerec trec_;
	int ndx_;
//	AppData* vodinfo_;
	void init();
};

// [VODTraceFile] Tcl handlers

static class VODTraceFileClass : public TclClass {
 public:
	VODTraceFileClass() : TclClass("VODTraceFile") {}
	TclObject* create(int, const char*const*) {
		return (new VODTraceFile());
	}
} class_vodtracefile;

VODTraceFile::VODTraceFile() : status_(0)
{
}

int VODTraceFile::command(int argc, const char*const* argv)
{

	if (argc == 3) {
		if (strcmp(argv[1], "filename") == 0) {
			name_ = new char[strlen(argv[2])+1];
			strcpy(name_, argv[2]);
			return(TCL_OK);
		}
	}
	return (NsObject::command(argc, argv));
}

void VODTraceFile::get_next(int& ndx, struct tracerec& t)
{
	t.trec_time = trace_[ndx].trec_time;
	t.trec_size = trace_[ndx].trec_size;
//	t.trec_ftype = trace_[ndx].trec_ftype;
	t.trec_dist = trace_[ndx].trec_dist;

	if (++ndx == nrec_)
		ndx = 0;
}

int VODTraceFile::setup()
{
	tracerec* t;
	struct stat buf;
	int i;
	FILE *fp;

	// only open/read the file once (could be shared by multiple SourceModels
	if (! status_) {
		status_ = 1;

		if (stat(name_, (struct stat *)&buf)) {
			printf("VODTraceFile: could not stat %s\n", name_);
			exit(-1);
		}

		// get the number of records in the file by the number of bytes in it
		nrec_ = buf.st_size/sizeof(tracerec);

		if ( (unsigned)( nrec_ * sizeof(tracerec) ) != buf.st_size) {
			printf("VODTraceFile: bad file size in %s\n", name_);
			exit(-1);
		}

		trace_ = new struct tracerec[nrec_];

		if ((fp = fopen(name_, "rb")) == NULL) {
			printf("VODTraceFile: can't open file %s\n", name_);
			exit(-1);
		}

//		fprintf(stderr, "Dumping trace file contents:\n");
		for (i = 0, t = trace_; i < nrec_; i++, t++)
			if (fread((char *)t, sizeof(tracerec), 1, fp) != 1) {
				printf("VODTraceFile: read failed\n");
				exit(-1);
			}
			else {
				t->trec_time = ntohl(t->trec_time);
				t->trec_size = ntohl(t->trec_size);
				t->trec_dist = ntohl(t->trec_dist);
//				t->trec_ftype = ntohl(t->trec_ftype);

				// debug
//				fprintf(stderr, "\n%f %d %f", (float)t->trec_time, t->trec_size, (float)t->trec_dist);
			}
//		fprintf(stderr, "\nDone.\n");

	}

	/* pick a random starting place in the trace file */
	return (int(Random::uniform((double)nrec_)+.5));

//	// forget about video randomization, start at the beginning
//	return 0;

}

void VODTraceFile::recv(Packet*, Handler*)
{
        /* shouldn't get here */
        abort();
}

// [Application/Traffic/VODTrace] Tcl handlers

static class VODTrafficTraceClass : public TclClass {
 public:
	VODTrafficTraceClass() : TclClass("Application/Traffic/VODTrace") {}
	TclObject* create(int, const char*const*) {
	        return(new VODTrafficTrace());
	}
} class_traffictrace;

VODTrafficTrace::VODTrafficTrace()
{
	tfile_ = (VODTraceFile *)NULL;
}

void VODTrafficTrace::init()
{
	if (tfile_)
		ndx_ = tfile_->setup();
}

int VODTrafficTrace::command(int argc, const char*const* argv)
{
	Tcl& tcl = Tcl::instance();

	if (argc == 3) {
		if (strcmp(argv[1], "attach-tracefile") == 0) {
			tfile_ = (VODTraceFile *)TclObject::lookup(argv[2]);
			if (tfile_ == 0) {
				tcl.resultf("no such node %s", argv[2]);
				return(TCL_ERROR);
			}
			return(TCL_OK);
		}
	}
	return (TrafficGenerator::command(argc, argv));

}

void VODTrafficTrace::timeout()
{
	if (! running_)
		return;

	// send a packet
	/* Note: May need to set "NEW_BURST" flag in sendmsg() for signifying
	 * a new talkspurt when using vat traces(see expoo.cc, tcl/ex/test-rcvr.tcl)
	 */
//	VideoData vodinfo_ ((float)trec_.trec_dist);

	//	PacketData vodinfo(10);
	AppData* vodinfo = new VideoData ((float)trec_.trec_dist);

	// the following sendmsg() expects an [Agent/UDP]
	// UdpAgent::sendmsg(int nbytes, AppData* data, const char *flags = 0)
	agent_->sendmsg(size_, vodinfo);
//	agent_->sendmsg(size_);

	// figure out when to send the next one
	// next_interval will fetch the next packet's size and store it in size_
	nextPkttime_ = next_interval(size_);


	// schedule it
	timer_.resched(nextPkttime_);
}

double VODTrafficTrace::next_interval(int& size)
{
	tfile_->get_next(ndx_, trec_);
	size = trec_.trec_size;

	return(((float)trec_.trec_time)/1000000.0); // usecs->secs
}

