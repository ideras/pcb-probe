#include <cmath>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <list>
#include "pcb-probe.h"

using namespace std;

PCBProbeInfo info;
list<GCodeCommand> cmdList;
map<string, int> cellVariables; //GCode parameters associated with every cell in the Grid
int nextVariableNumber = 2000;
int currentLine = 0;

/*
 * This routine will split up long distances into chunks
 * if recursively halves the values (so we don't have to
 * worry about being proportional)
 */
static void distance_split(Real from_x, Real from_y, GCodeCommand &command)
{
    Real to_x = command.getXCoord();
    Real to_y = command.getYCoord();

    Real dist_x = to_x - from_x;
    Real dist_y = to_y - from_y;

    if (fabs(dist_x) > info.SplitOver || fabs(dist_y) > info.SplitOver) {
        Real mp_x = from_x + (dist_x / 2);
        Real mp_y = from_y + (dist_y / 2);

        GCodeCommand cmd1(command.name, mp_x, mp_y);
        GCodeCommand cmd2(command.name, to_x, to_y);

        if (command.hasFeedRate())
            cmd1.setFeedRate(command.getFeedRate());


        distance_split(from_x, from_y, cmd1);
        distance_split(mp_x, mp_y, cmd2);
    } else {
        cmdList.push_back(command);
    }
}

static void split_if_needed(GCodeCommand &command)
{
    if (info.Pos.z >= 0 || !command.hasXCoord() || !command.hasYCoord()) {
        cmdList.push_back(command);
    } else {

        /*
         * We now have a start and end position, we can call our recursive routine...
         */
        distance_split(info.Pos.x, info.Pos.y, command);
    }
}

static inline void moveTo(GCodeCommand &command)
{
    if (command.hasXCoord())
        info.Pos.x = command.getXCoord();

    if (command.hasYCoord())
        info.Pos.y = command.getYCoord();

    if (command.hasZCoord())
        info.Pos.z = command.getZCoord();
}

void LoadAndSplitSegments(const char *infile_path)
{
    string line;
    ifstream in(infile_path);

    if (!in.is_open()) {
        cerr << "Unable to open file: " << infile_path << endl;
        return;
    }

    GCodeCommand cmd;
    bool definedMillMinX = false;
    bool definedMillMaxX = false;
    bool definedMillMinY = false;
    bool definedMillMaxY = false;
    bool definedMillRouteDepth = false;
	bool definedDrillSpotDepth = false;

    info.ResetPos();
    info.SplitOver = info.GridSize;
	info.HasDrillSpots = false;

    while (in.good()) {

        currentLine++;
        getline(in, line);
        ParseGCodeLine(line, cmd);
        
        if (cmd.name == "G20") {
            //Units in inches
            info.GridSize = info.GridSize / 25.4;
            info.SplitOver = info.SplitOver / 25.4;
            info.UnitType = UNIT_INCHES;
        } else if (cmd.name == "G21") {
            info.UnitType = UNIT_MM;
        }

        if (cmd.name == "G00" || cmd.name == "G01") {
            /*
             * We have a move command, if our z is below zero then this will
             * count towards our area
             */
            split_if_needed(cmd);
            moveTo(cmd);

            if (info.Pos.z < 0) {
                if (!definedMillRouteDepth || (info.Pos.z < info.MillRouteDepth))
                    info.MillRouteDepth = info.Pos.z;

                if (!definedMillMinX || (info.Pos.x < info.MillMinX)) {
                    definedMillMinX = true;
                    info.MillMinX = info.Pos.x;
                }
                if (!definedMillMaxX || (info.Pos.x > info.MillMaxX)) {
                    definedMillMaxX = true;
                    info.MillMaxX = info.Pos.x;
                }
                if (!definedMillMinY || (info.Pos.y < info.MillMinY)) {
                    definedMillMinY = true;
                    info.MillMinY = info.Pos.y;
                }
                if (!definedMillMaxY || (info.Pos.y > info.MillMaxY)) {
                    definedMillMaxY = true;
                    info.MillMaxY = info.Pos.y;
                }
            }

		} else if (cmd.name == "G82") {
			moveTo(cmd);

			info.HasDrillSpots = true;
			if (!definedDrillSpotDepth && cmd.hasZCoord())
				info.DrillSpotDepth = cmd.getZCoord();

			cmdList.push_back(cmd);

		} else if (!cmd.name.empty()) {
            cmdList.push_back(cmd);
        }
    }
    in.close();

	info.GridMaxX = (unsigned int)ceil((info.MillMaxX - info.MillMinX) / info.GridSize);
    info.GridMaxY = (unsigned int)ceil((info.MillMaxY - info.MillMinY) / info.GridSize);

	info.MillMinX -= info.GridSize / 2.0;
	info.MillMinY -= info.GridSize / 2.0 ;

	info.Gx = (info.MillMaxX - info.MillMinX)/(info.GridMaxX + 0.5);
	info.Gy = (info.MillMaxY - info.MillMinY)/(info.GridMaxY + 0.5);
}

//Second Pass

string getKey(int gx, int gy)
{
    stringstream ss;

    ss << gx << "," << gy;
    return ss.str();
}

/*
 * This makes sure we have a variable assigned to a given cell
 */
void ensure_cell_variable(int gx, int gy)
{
    string key = getKey(gx, gy);

    if (cellVariables.find(key) == cellVariables.end()) {
        cellVariables[key] = nextVariableNumber++;
    }
}

/*
 * This functions returns true if a cell has a variable associated, false otherwise
 */
bool cellHasVariable(int gx, int gy)
{
    string key = getKey(gx, gy);

    return (cellVariables.find(key) != cellVariables.end());
}

int cell_variable(int gx, int gy)
{
    string key = getKey(gx, gy);
    return cellVariables[key];
}

/*
 * Given a co-ordinate we can work out a grid x and y number
 * so we can lookup stuff 
 */

void grid_ref(Real x, Real y, unsigned int &ref_x, unsigned int &ref_y)
{

    Real zero_x = x - info.MillMinX;
    Real zero_y = y - info.MillMinY;

    ref_x = (unsigned int)floor(zero_x / info.Gx);
    ref_y = (unsigned int)floor(zero_y / info.Gy);
}

/*
 * Given a co-ordinate we need to interpolate the values from
 * the surrounding cells
 */
string interpolate(Real x, Real y, bool isLinearMotionCommand)
{

    unsigned int cellx, celly;
    grid_ref(x, y, cellx, celly);

    Real os_x = ((x - info.MillMinX) - ((Real) cellx * info.Gx)) / info.Gx;
    Real os_y = ((y - info.MillMinY) - ((Real) celly * info.Gy)) / info.Gy;

    unsigned int px_cell = cellx + (os_x > 0.5 ? 1 : -1);
    unsigned int py_cell = celly + (os_y > 0.5 ? 1 : -1);

    if (px_cell < 0 || px_cell > info.GridMaxX) {
        px_cell = cellx;
    }
    if (py_cell < 0 || py_cell > info.GridMaxY) {
        py_cell = celly;
    }

    Real x_pc = 0.5 + (os_x > 0.5 ? 1 - os_x : os_x);
    Real y_pc = 0.5 + (os_y > 0.5 ? 1 - os_y : os_y);

    /*
     * Now we can make sure that each of our cells has a variable in it...
     */
    ensure_cell_variable(cellx, celly);
    ensure_cell_variable(px_cell, celly);
    ensure_cell_variable(cellx, py_cell);
    ensure_cell_variable(px_cell, py_cell);

    /*
     * Now we can work out the interpolation...
     */
	stringstream ss;
	string depthParameter = isLinearMotionCommand? "#3" : "#7";

    ss.precision(3);
    ss << fixed << (x_pc * y_pc) << "*#" << cell_variable(cellx, celly) << " + " <<
            ((1 - x_pc) * y_pc) << "*#" << cell_variable(px_cell, celly) << " + " <<
            (x_pc * (1 - y_pc)) << "*#" << cell_variable(cellx, py_cell) << " + " <<
            ((1 - x_pc) * (1 - y_pc)) << "*#" << cell_variable(px_cell, py_cell) << " + " <<
            depthParameter;

    return ss.str();
}

/*
 * Last phase ... add the depth sensing bit to the file
 */
void DoInterpolation()
{
    list<GCodeCommand>::iterator it = cmdList.begin();

    info.ResetPos();
    while (it != cmdList.end()) {
        GCodeCommand cmd = *it;

        if (cmd.name == "G00" || cmd.name == "G01") {
            /*
             * We have a move command, if our z is below zero then this will
             * count towards our area
             */
            moveTo(cmd);

            if (info.Pos.z < 0) {
                /*
                 * First thing we do is allocate a variable number to the grid
                 * square (if it hasn't already got one)
                 */
                string zformula = interpolate(info.Pos.x, info.Pos.y, true);
                it->setZFormula(zformula);
            }

		} else if (cmd.name == "G82") {
			moveTo(cmd);

			string zformula = interpolate(info.Pos.x, info.Pos.y, false);
			it->setZFormula(zformula);
		}

        it++;
    }

}

void GenerateGCodeWithProbing(const char *outfile_path)
{
    ofstream out(outfile_path);

    if (!out.is_open()) {
        cerr << "Unable to open file: " << outfile_path << endl;
        return;
    }

    list<GCodeCommand>::iterator it = cmdList.begin();

    while (it != cmdList.end()) {
        GCodeCommand cmd = *it;

        /*
         * We'll put our stuff right after the G21
         */
        if (cmd.name == "G21" || cmd.name == "G20") {
            Real clear_height;
            Real traverse_height; //Traverse height
            Real probe_depth;     //Probe max depth, stop at this position if not triggered
            Real initial_probe;   // Initial probe Z position to find worksurface
            Real traverse_speed; // Traver Speed
            Real probe_speed; //Probe Speed
                
            if (info.UnitType == UNIT_INCHES) {
                clear_height = 0.47244;
                traverse_height = 0.01969;
                probe_depth = -0.03937;
                initial_probe = -0.1969;
                traverse_speed = 400 / 25.4;
                probe_speed = 60/25.4;
            } else {
                clear_height = 12.0;
                traverse_height = 0.5;
                probe_depth = -1;
                initial_probe = -5;
                traverse_speed = 400;
                probe_speed = 60;

            }
            
            out << cmd.ToString() << endl;
            out << "\n"
                    "(Processed with pcb-probe by Ivan de Jesus Deras 2013 [Lee Essen, 2011] )"
                    "\n"
                    "\n"
                    "#1=" << clear_height << "			(clearance height)\n"
                    "#2=" << traverse_height << "			(traverse height)\n"
                    "#3=" << info.MillRouteDepth << "		(route depth)\n"
                    "#4=" << probe_depth << "			(probe depth)\n"
                    "#5=" << traverse_speed << "			(traverse speed)\n"
                    "#6=" << probe_speed << "			(probe speed)\n";
			
	    if (info.HasDrillSpots)
		out << "#7=" << info.DrillSpotDepth << "		(drill spot depth)\n";

	    out << endl << endl;
            out <<  "M05			(stop motor)\n"
                    "(MSG,PROBE: Position to within 5mm [~0.2 inches] of surface & resume)\n"
                    "M60			(pause, wait for resume)\n"
                    "G49			(clear any tool offsets)\n"
                    "G92.1			(zero co-ordinate offsets)\n"
                    "G91			(use relative coordinates)\n"
                    "G38.2 Z" << initial_probe << " F[#6]	(probe to find worksurface)\n"
                    "G90			(back to absolute)\n"
                    "G92 Z0			(zero Z)\n"
                    "G00 Z[#1]		(safe height)\n"
                    "(MSG,PROBE: Z-Axis calibrate complete, beginning probe)\n"
                    "\n"
                    "(probe routine)\n"
                    "(params: x y traverse_height probe_depth traverse_speed probe_speed)\n"
                    "O100 sub\n"
                    "G00 X[#1] Y[#2] Z[#3] F[#5]\n"
                    "G38.2 Z[#4] F[#6]\n"
                    "G00 Z[#3]\n"
                    "O100 endsub\n"
                    "\n";

            /*
             * Now we can create the code for the depth sensing bit... but
             * we should do it in a fairly optimal way
             */

            unsigned int gx, gy, rgx;

            for (gy = 0; gy <= info.GridMaxY; gy++) {
                for (rgx = 0; rgx <= info.GridMaxX; rgx++) {
                    if (gy & 1) {
                        gx = info.GridMaxX - rgx;
                    } else {
                        gx = rgx;
                    }

                    // Find the point in the centre of the grid square...
                    Real px = info.MillMinX + ((Real) gx * info.Gx) + (info.Gx / 2);
                    Real py = info.MillMinY + ((Real) gy * info.Gy) + (info.Gy / 2);

                    if (!cellHasVariable(gx, gy))
                        continue;

                    int var = cell_variable(gx, gy);

                    out << "(PROBE[" << gx << "," << gy << "] " << px << " " << py << " -> " << var << ")" << endl;
                    out << "O100 call [" << px << "] [" << py << "] [#2] [#4] [#5] [#6]" << endl;
                    out << "#" << var << " = #5063" << endl;
                }
            }

            /*
             * Now before we go into the main mill bit we need to give you a chance
             * to undo the probe connections
             */
            out << "\n"
                    "\n"
                    "G00 Z[#1]		(safe height)\n"
                    "(MSG,PROBE: Probe complete, remove connections & resume)\n"
                    "M60			(pause, wait for resume)\n"
                    "(MSG,PROBE: Beginning etch)\n"
                    "\n"
                    "\n";
        } else {
            out << cmd.ToString() << endl;
        }

        it++;
    }
}