#include <iostream>
#include <fstream>
#include <string>
#include <vector>

	/*
	 * This application will receive a terse video trace file containing the video's frame sizes and
	 * delta arrival times, and will generate a packetized version of that video stream, splitting
	 * each video frame in network packets of a size supplied by the user. It is capable of
	 * outputting the result as both a plain-text tab-delimited file, or as a binary file suitable
	 * for ns2's Traffic Generator [Application/Traffic/Trace].
	 */

enum OutputFormat { ASCII, BINARY };

int main(int argc, char** argv){
	using namespace std;

	// TODO: read option from cmd line
	OutputFormat format_ = BINARY;

	if(argc == 1) {
		cerr << "Error: Please supply a file to process." << '\n';
		cerr << "Usage: " << argv[0] << " filename.terse.trace packetsize" << '\n';
		exit(1);
	} else if(argc == 2) {
		cerr << "Error: Please supply a packet size." << '\n';
		cerr << "Usage: " << argv[0] << " filename.terse.trace packetsize" << '\n';
		exit(1);
	} else if(argc > 3) {
		cerr << "Error: Incorrect number of input parameters." << '\n';
		cerr << "Usage: " << argv[0] << " filename.terse.trace packetsize" << '\n';
		exit(1);
	}

	string inFilename(argv[1]);
	string outFilename = inFilename + ".ns2"; // append .ns2 to the input filename

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
	unsigned psize ( atoi(argv[2]) );
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
	// TODO: explain
	unsigned packetcount = 0;

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

		unsigned packets = 0;
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
	cout << trace_time_.size() << " entries processed from " << inFilename << '\n';
	cout << "Frame size: " << psize << ", output frame count: " << packetcount << '\n';
	cout << "Output saved in " << outFilename << '\n';
	cout << "All done" << '\n';

	return 0;
}
