#include <iostream>
#include <fstream>
#include <string>
#include <vector>

	/*
	 * This program will receive a terse video trace file containing the video's frame sizes and
	 * delta arrival times, and will generate a packetized version of that video stream, splitting
	 * each video frame in network packets of a size supplied by the user. It is capable of
	 * outputting the result as both a plain-text tab-delimited file, or as a binary file suitable
	 * for ns2's Traffic Generator [Application/Traffic/Trace].
	 */

int main(int argc, char** argv){
	using namespace std;

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
	ifstream inFile (inFilename.c_str(), ios::in);
	if (!inFile.is_open()) {
	    cerr << "Error: Unable to open file " << inFilename << " for reading." <<'\n';
	    exit(1);
	}

	// open the output file
	ofstream outFile (outFilename.c_str(), ios::out | ios::trunc);
	if (!outFile.is_open()) {
	    cerr << "Error: Unable to open file " << outFilename << " for writing." << '\n';
	    exit(1);
	}

	// get the frame size
	unsigned fsize ( (unsigned) atoi(argv[2]) );
	// TODO: write checks for the frame size

	unsigned time_;
	unsigned len_;
	vector< unsigned > trace_time_;
	vector< unsigned > trace_len_;

	/* some trace files store the 'length' field with a dot (.) separating thousands
	 * the following code reads each line, erases all '.' characters from it, then
	 * reads the time and length from that line into the time_ and length_ vectors
	 */
	string line_;
	while ( !inFile.eof() ) {
		// read a line
		getline (inFile, line_);

		// erase all dots from the line read
		string::size_type loc = line_.find( '.', 0 );
		while ( loc != string::npos ) {
			line_.erase(loc, 1);
			loc = line_.find ('.', loc); // find the next dot char in the line
		}

		// get the {time,length} pairs and store them in the vectors
		sscanf(line_.c_str(), "%u\t%u", &time_, &len_);
		trace_time_.push_back(time_);
		trace_len_.push_back(len_);
	}

	// TODO: sort the {time,length} pairs (atm we assume the input trace is sorted)

	// algorithm
	for(unsigned i=0; i < trace_time_.size(); i++){

	}

	// write
	for(unsigned i=0; i < trace_time_.size(); i++){
		outFile << trace_time_[i] << '\t' << trace_len_[i] << '\n';
	}

	// close all files
	inFile.close();
	outFile.close();

	// print some statistics
	cout << trace_time_.size() << " entries processed from " << inFilename << '\n';
	cout << "Frame size: " << fsize << '\n';
	cout << "Output saved in " << outFilename << '\n';
	cout << "All done" << '\n';

	return 0;
}


/*
awk 'BEGIN{rem = 21667} \
{pkt = int(($1/8 + 100)/200); if (pkt == 0) pkt = 1;\
 ia = int(20000/pkt); print rem, 200; rem = 41667;\
 for (i = 1; i < pkt; i++) { print ia, 200; rem -= ia};\
 }'
 */
