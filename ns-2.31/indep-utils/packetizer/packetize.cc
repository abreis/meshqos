/* Copyright (C) 2009 Andre Braga Reis
 * andrebragareis at gmail dot com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

	/*
	 * This application will receive a terse video trace file containing the video's frame sizes and
	 * delta arrival times, and will generate a packetized version of that video stream, splitting
	 * each video frame in network packets of a size supplied by the user. It is capable of
	 * outputting the result as both a plain-text tab-delimited file, or as a binary file suitable
	 * for ns2's Traffic Generator [Application/Traffic/Trace].
	 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "getopt_pp.h"

	/* TODO: support accounting for size of streaming protocol headers
	 * TODO: support fetching all fields in verbose trace files (atm only frame type)
	 * TODO: support grouping of multiple frames inside a single packet
	 */

enum InputFormat { TERSE, VERBOSE };
enum OutputFormat { ASCII, BINARY };

int main(int argc, char** argv){
	using namespace std;
	using namespace GetOpt;

    GetOpt_pp args (argc, argv);

    // help function
    if ( args >> OptionPresent('h', "help") ) {
    	cout << "Usage: packetize tracefile [-p packetsize] [-o outputfile] [-a|-b]\n"
				"\nOptions:\n"
				"  -p, --packet-size \tSize, in bytes, of output network packets\n"
    			"  -o, --out \t\tFile in which to write results\n"
				"  -t, --terse \t\tInput trace is in terse format\n"
				"  -v, --verbose \tInput trace is in verbose format\n"
    			"  -a, --ascii \t\tOutput in plain-text mode\n"
				"  -b, --binary \t\tOutput in binary mode, suitable for ns2"
				<< endl;
    	exit(0);
    }

    // get input type
    bool inputterse, inputverbose;
	args >> OptionPresent('t', "terse", inputterse);
	args >> OptionPresent('v', "verbose", inputverbose);

    InputFormat informat_ = TERSE; 	// default to terse input
	if( inputverbose && !inputterse ) informat_ = VERBOSE;
	else if ( inputverbose && inputterse) {
		cerr << "Error: Please supply only one of '--terse' and '--verbose'" << '\n';
		cerr << "Try `packetize --help' for more information." << endl;
		exit(1);
	}

    // get output type
    bool outputbin, outputascii;
	args >> OptionPresent('b', "binary", outputbin);
	args >> OptionPresent('a', "ascii", outputascii);

    OutputFormat outformat_ = BINARY; 	// default to binary output
	if( outputascii && !outputbin ) outformat_ = ASCII;
	else if ( outputascii && outputbin) {
		cerr << "Error: Please supply only one of '--binary' and '--ascii'" << '\n';
		cerr << "Try `packetize --help' for more information." << endl;
		exit(1);
	}

	// get input / output filenames
	string inFilename, outFilename;
	args >> Option(GetOpt_pp::EMPTY_OPTION, inFilename);
	if ( inFilename == "" ) {
		cerr << "Error: Please supply an input trace file." << '\n';
		cerr << "Try `packetize --help' for more information." << endl;
		exit(1);
	}
	if ( args >> OptionPresent('o', "out") )
		args >> Option('o', "out", outFilename);
	else {
		outFilename = inFilename + ".ns2"; // append .ns2 to the input filename
	}

	// open the input file
	ifstream inFile (inFilename.c_str());
	if (!inFile.is_open()) {
	    cerr << "Error: Unable to open file " << inFilename << " for reading." <<'\n';
	    exit(1);
	}

	// open the output file
	ofstream outFile;
	if( outformat_ == BINARY) {
		outFile.open (outFilename.c_str(), ios::trunc | ios::binary);
	} else if (outformat_ == ASCII) {
		outFile.open (outFilename.c_str(), ios::trunc);
	}
	if (!outFile.is_open()) {
	    cerr << "Error: Unable to open file " << outFilename << " for writing." << '\n';
	    exit(1);
	}

	// get the packet size
	unsigned psize = 200; 	// default packet size set to 200 bytes
	args >> Option('p', "packet-size", psize);
	// TODO: write checks for the packet size

	unsigned len_;
	float time_;
	vector< unsigned > trace_len_;
	vector< float > trace_time_;
	vector < unsigned > trace_ftype_;

	if(informat_ == TERSE) {
		// read the {length,time} pairs from inFile into the vectors
		while ( inFile >> len_ && inFile >> time_ ) {
			trace_len_.push_back(len_);
			trace_time_.push_back(time_);
		}
	} else if(informat_ == VERBOSE) {
		int fnumber_;
		char ftype_;
		float unknown_, psnr1_, psnr2_;

		// read the {length,time,frametype} values from inFile into the vectors
		while ( inFile >> fnumber_ && inFile >> unknown_ &&
				inFile >> ftype_ && inFile >> len_ && inFile >> time_ &&
				inFile >> psnr1_ && inFile >> psnr2_) {
			trace_len_.push_back(len_);
			trace_time_.push_back(time_);
			trace_ftype_.push_back( (unsigned) ftype_);
		}
	}

	// TODO: sort the {length,time} pairs (atm we assume the input trace is sorted)

	/* packetization algorithm
	 * ns2 TrafficTrace expects a trace file with 2 32-bit fields in big-endian byte order.
	 * The first field contains the time in microseconds until the next packet is generated,
	 * and the second field contains the packet size in bytes (from the ns2 manual).
	 */
	unsigned packetcount = 0;
	cout << "Processing " << trace_time_.size() << " frames..." << '\n';
	for(unsigned i=0; i < trace_time_.size(); i++){
		vector< unsigned > out_time_;
		vector< unsigned > out_len_;
		unsigned out_ftype_;
		if(informat_ == VERBOSE)
			out_ftype_ = trace_ftype_[i];

		// uncomment this line to get some debug help into the output file (ASCII only)
		//outFile << "*** " << trace_time_[i] << '\t' << trace_len_[i] << endl;

		unsigned nexttime_ (trace_time_[i] * 1000); 	// convert from ms to us
		unsigned nextsize_ (trace_len_[i] / 8); 	// convert from bits to bytes
		unsigned npkts (nextsize_ / psize); 	// the number of packets to split this frame into
		npkts++; 	// round up the number of frames (to account for a smaller last frame)

		unsigned remsize (nextsize_); 	// remaining frame length to packetize
		unsigned timefraction ( nexttime_ / npkts );	//

		// due to rounding errors, the cumulative time of these packets might be different from the frame time
		// finaltime makes up for this by increasing the last packet arrival time, if necessary
		unsigned finaltime ( timefraction + (nexttime_ - timefraction*npkts) );

		unsigned packets = 0;	// number of packets generated from this frame
		while (remsize > psize) {
			out_time_.push_back(timefraction);
			out_len_.push_back(psize);
			remsize -= psize; // decrease remaining size
			packets++;	// increase packet count for this frame
		}
		// push the last packet
		out_time_.push_back(finaltime);
		out_len_.push_back(remsize);
		packets++;

		// statistics
		packetcount += packets;

		// immediately output the resulting frames, to keep the memory usage small
		if( outformat_ == BINARY ) {
			for(unsigned i=0; i < out_time_.size(); i++){
				unsigned memtime_ = htonl(out_time_[i]);	// convert to bigendian (network byte order)
				unsigned memlen_ = htonl(out_len_[i]);
				unsigned memftype_;
				if(informat_ == VERBOSE)
					memftype_ = htonl(out_ftype_);

				outFile.write( (char*) &memtime_, 4);	// write 2*32 bits (4 bytes) into the file
				outFile.write( (char*) &memlen_, 4);
				if(informat_ == VERBOSE)
					outFile.write( (char*) &memftype_, 4); // write 32 bits with frame type
			}
		} else if ( outformat_ == ASCII) {
			for(unsigned i=0; i < out_time_.size(); i++){
				outFile << (char)out_ftype_ << '\t' << out_time_[i] << '\t' << out_len_[i] << '\n';
			}
		}
	}

	// close all files
	inFile.close();
	outFile.close();

	// print some information
	cout << '\n' << trace_time_.size() << " entries processed from '" << inFilename << "'\n";
	cout << "Packet size: " << psize << ", output packet count: " << packetcount << '\n';
	cout << "Output saved in '" << outFilename << "'\n";

	return 0;
}
