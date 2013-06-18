/* 
 * File:   main.cpp
 * Author: ideras
 *
 * Created on January 18, 2013, 5:46 PM
 */

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string>
#include <fstream>
#include "parser.h"
#include "pcb-probe.h"

using namespace std;

int main(int argc, char** argv) {
	char *infile_path, *outfile_path;

    if ((argc != 3) && (argc != 4)) {
        cerr << "Usage: " << argv[0] << " [<grid size in mm>] infile outfile" << endl;
        exit(1);
    }

	info.GridSize = 5; //5 mm by default

	if (argc == 3) {
		infile_path = argv[1];
		outfile_path = argv[2];
	} else {
        double gsize = atof(argv[1]);
		
		info.GridSize = gsize == 0.0? 5.0 : gsize;
		infile_path = argv[2];
		outfile_path = argv[3];
	}

    cout << "Processing input file ... " << infile_path << endl;
    LoadAndSplitSegments(infile_path);
    
    string unit = (info.UnitType == UNIT_INCHES)? "Inches" : "mm";
    cout << "Board Size (" << unit << "): " << fabs(info.MillMaxX - info.MillMinX) << "x" << fabs(info.MillMinY - info.MillMaxY) << endl << endl;
    
    cout << "Generating GCode output in " << outfile_path;
    DoInterpolation();
    cout << " ." << endl;
    GenerateGCodeWithProbing(outfile_path);
    cout << "Done." << endl;
    
    return 0;
}

