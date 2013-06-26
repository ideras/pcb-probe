/* 
 * File:   pcb-gcode.h
 * Author: ideras
 *
 * Created on January 18, 2013, 8:40 PM
 */

#ifndef PCB_GCODE_H
#define	PCB_GCODE_H

#include "parser.h"

#define UNIT_INCHES     0
#define UNIT_MM         1

struct Position {
    Real x;
    Real y;
    Real z;
};

struct PCBProbeInfo
{
    int UnitType; //MM by default
    Position Pos;
    double GridSize;
	double Gx, Gy; //Adjusted GridSize on X and Y axes
    double SplitOver;
    
    //Board boundaries
    Real MillMinX;
    Real MillMinY;
    Real MillMaxX;
    Real MillMaxY;

    //Route Depth
    Real MillRouteDepth;

	//Drill Spots Fields
	bool HasDrillSpots;
	Real DrillSpotDepth;
    
    //Number of cells in Grid
    unsigned int GridMaxX;
    unsigned int GridMaxY;
    
    void ResetPos()
    {
        Pos.x = 0;
        Pos.y = 0;
        Pos.z = 0;
    }
    
};

extern PCBProbeInfo info;

void LoadAndSplitSegments(const char *infile_path);
void DoInterpolation();
void GenerateGCodeWithProbing(const char *outfile_path);


#endif	/* PCB_GCODE_H */

