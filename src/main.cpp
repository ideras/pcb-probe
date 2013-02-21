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

    if (argc != 3) {
        cout << "Usage: " << argv[0] << " " << "infile outfile" << endl;
        exit(1);
    }

    char *infile_path = argv[1];
    char *outfile_path = argv[2];

    cout << "Processing input file ... " << infile_path << endl;
    LoadAndSplitSegments(infile_path, "o1");
    
    string unit = (info.UnitType == UNIT_INCHES)? "Inches" : "mm";
    cout << "Board Size (" << unit << "): " << fabs(info.MillMaxX - info.MillMinX) << "x" << fabs(info.MillMinY - info.MillMaxY) << endl << endl;
    
    cout << "Generating GCode output in " << outfile_path << endl;
    DoInterpolation("o2");
    GenerateGCodeWithProbing(outfile_path);
    cout << "Done." << endl;
    
    return 0;
}

