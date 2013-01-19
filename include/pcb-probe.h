/* 
 * File:   pcb-gcode.h
 * Author: ideras
 *
 * Created on January 18, 2013, 8:40 PM
 */

#ifndef PCB_GCODE_H
#define	PCB_GCODE_H

#include "parser.h"

struct Position {
    Real x;
    Real y;
    Real z;
};

struct PCBProbeInfo
{
    Position Pos;
    double GridSize;
    double SplitOver;
    
    //Board boundaries
    Real MillMinX;
    Real MillMinY;
    Real MillMaxX;
    Real MillMaxY;
    
    //Number of cells in Grid
    int GridMaxX;
    int GridMaxY;
    
    void ResetPos()
    {
        Pos.x = 0;
        Pos.y = 0;
        Pos.z = 0;
    }
    
};

extern PCBProbeInfo info;

void LoadAndSplitSegments(const char *infile_path, const char *outfile_path);
void DoInterpolation(const char *outfile_path);
void GenerateGCodeWithProbing(const char *outfile_path);


#endif	/* PCB_GCODE_H */

