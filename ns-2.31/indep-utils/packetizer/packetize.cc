/* GPL */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "getopt_pp.h"

	/*
	 * This application will receive a terse video trace file containing the video's frame sizes and
	 * delta arrival times, and will generate a packetized version of that video stream, splitting
	 * each video frame in network packets of a size supplied by the user. It is capable of
	 * outputting the result as both a plain-text tab-delimited file, or as a binary file suitable
	 * for ns2's Traffic Generator [Application/Traffic/Trace].
	 */

	/*
	 * TODO: support verbose trace files
	 * TODO: write usage information for --help
	 */

enum OutputFormat { ASCII, BINARY };

int main(int argc, char** argv){
	using namespace std;
	using namespace GetOpt;

    GetOpt_pp args (argc, argv);

    // get output type
    bool outputbin, outputascii;
	args >> OptionPresent('b', "binary", outputbin);
	args >> OptionPresent('a', "ascii", outputascii);

    OutputFormat format_ = BINARY; 	// default to binary output
	if( outputascii && !outputbin ) format_ = ASCII;
	else if ( outputascii && outputbin) {
		cerr << "Error: Please supply only one of '--binary' and '--ascii'" << endl;
		exit(1);
	}

	// get input / output filenames
	string inFilename, outFilename;
	args >> Option(GetOpt_pp::EMPTY_OPTION, inFilename);
	if ( inFilename == "" ) {
		cerr << "Error: Please supply an input trace file." << endl;
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
	if( format_ == BINARY) {
		outFile.open (outFilename.c_str(), ios::trunc | ios::binary);
	} else if (format_ == ASCII) {
		outFile.open (outFilename.c_str(), ios::trunc);
	}
	if (!outFile.is_open()) {
	    cerr << "Error: Unable to open file " << outFilename << " for writing." << '\n';
	    exit(1);
	}

	// get the packet size
	unsigned psize = 200; 	// default packet size set to 200 bytes
	args >> Option('p', "packetsize", psize);
	// TODO: write checks for the packet size

	unsigned len_;
	float time_;
	vector< unsigned > trace_len_;
	vector< float > trace_time_;

	// read the {length,time} pairs from inFile into the vectors
	while ( inFile >> len_ && inFile >> time_ ) {
		trace_len_.push_back(len_);
		trace_time_.push_back(time_);
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
		if( format_ == BINARY ) {
			for(unsigned i=0; i < out_time_.size(); i++){
				unsigned memtime_ = htonl(out_time_[i]);	// convert to bigendian (network byte order)
				unsigned memlen_ = htonl(out_len_[i]);
				outFile.write( (char*) &memtime_, 4);	// write 32 bits (4 bytes) into the file
				outFile.write( (char*) &memlen_, 4);
			}
		} else if ( format_ == ASCII) {
			for(unsigned i=0; i < out_time_.size(); i++){
				outFile << out_time_[i] << '\t' << out_len_[i] << '\n';
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
