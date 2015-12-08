#include "UAV.h"


using namespace easymath;

UAV::UAV(XY start_loc, XY end_loc, UAVType t, TypeAStarAbstract* highPlanners, SectorAStarGrid* lowPlanners):
	highPlanners(highPlanners),lowPlanners(lowPlanners),loc(start_loc), end_loc(end_loc), type_ID(t),speed(1.0)
{
	static int calls=0;
	ID = calls++;
	sectors_touched.insert(curSectorID());
};

std::list<int> UAV::getBestPath(int memstart, int memend){
	return highPlanners->search(memstart, memend, type_ID);
}

void UAV::planAbstractPath(){
	sectors_touched.insert(curSectorID());
	list<int> high_path = highPlanners->search(curSectorID(), endSectorID(), type_ID);
	if (high_path_prev!=high_path){
		pathChanged=true;
		high_path_prev = high_path;
	} else {
		pathChanged=false;
	}
}

void UAV::planDetailPath(){
	int memstart = lowPlanners->getMembership(loc);
	int memend =  lowPlanners->getMembership(end_loc);
	list<int> high_path = getBestPath(memstart, memend);
	if (high_path.size()==0){
		printf("no path found!");
		system("pause");
	}
	high_path_prev = high_path;
	

	int memnext = nextSectorID();

	XY waypoint;
	if (memnext==memend){
		waypoint = end_loc;
	} else {
		waypoint = highPlanners->getLocation(memnext);
	}

	// TODO: VERIFY HERE THAT THE HIGH_PATH_BEGIN() IS THE NEXT MEMBER... NOT THE FIRST...
	// target the center of the sector, or the goal if it is reachable
	vector<XY> low_path = lowPlanners->search(loc,waypoint);

	if (low_path.empty()) low_path.push_back(loc); // stay in place...

	while (target_waypoints.size()) target_waypoints.pop(); // clear the queue;
	for (std::vector<XY>::reverse_iterator i=low_path.rbegin(); i!=low_path.rend(); i++) target_waypoints.push(*i); // adds waypoints to visit
	target_waypoints.pop(); // remove CURRENT location from target	

}

int UAV::getDirection(){
	// Identifies whether traveling in one of four cardinal directions
	return cardinalDirection(loc-highPlanners->getLocation(nextSectorID()));
}

void UAV::moveTowardNextWaypoint(){
	if (!target_waypoints.size()) return; // return if no waypoints
	for (int i=0; i<speed; i++){
		loc = target_waypoints.front();
		target_waypoints.pop();
	}
}