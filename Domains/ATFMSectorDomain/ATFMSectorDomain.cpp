#include "ATFMSectorDomain.h"

using namespace std;
using namespace easymath;

ATFMSectorDomain::ATFMSectorDomain(bool deterministic):
	is_deterministic(deterministic), conflict_thresh(1.0)
{


	pathTraces = new vector<vector<XY> >(); // traces the path (reset each epoch)

	// Object creation
	sectors = new vector<Sector>();
	fixes = new vector<Fix>();

	// inheritance elements: constant
	//n_control_elements=4; // 4 outputs for sectors (cost in cardinal directions) (no types)
	
	n_control_elements=4*UAV::NTYPES;
	n_state_elements=4; // 4 state elements for sectors ( number of planes traveling in cardinal directions)
	n_steps=100; // steps of simulation time
	n_types=UAV::NTYPES;

	// Read in files for sector management
	Load::load_variable(&membership_map,"agent_map/membership_map.csv");
	matrix2d agent_coords = FileManip::readDouble("agent_map/agent_map.csv");
	matrix2d connection_map = FileManip::readDouble("agent_map/connections.csv");
	matrix2d fix_locs = FileManip::readDouble("agent_map/fixes.csv");
	

	// Add sectors
	agent_locs = vector<XY>(agent_coords.size()); // save this for later Astar recreation
	for (unsigned int i=0; i<agent_coords.size(); i++){
		sectors->push_back(Sector(XY(agent_coords[i][0],agent_coords[i][1]),i));
		agent_locs[i] = sectors->back().xy;
	}

	// Adjust the connection map to be the edges
	// preprocess boolean connection map
	vector<AStarManager::Edge> edges;
	for (unsigned int i=0; i<connection_map.size(); i++){
		for (unsigned int j=0; j<connection_map[i].size(); j++){
			if (connection_map[i][j] && i!=j){
				edges.push_back(AStarManager::Edge(i,j));
			}
		}
	}

	planners = new AStarManager(UAV::NTYPES, edges, membership_map, agent_locs);

	// initialize fixes
	for (unsigned int i=0; i<fix_locs.size(); i++){
		fixes->push_back(Fix(XY(fix_locs[i][0],fix_locs[i][1]),i,is_deterministic,planners));
	}

	n_agents = sectors->size(); // number of agents dictated by read in file
	fixed_types=vector<int>(n_agents,0);

	conflict_count = 0; // initialize with no conflicts
	conflict_count_map = new ID_grid(planners->obstacle_map->dim1(), planners->obstacle_map->dim2());

}

ATFMSectorDomain::~ATFMSectorDomain(void)
{
	delete fixes;
	delete sectors;
	delete planners;
	delete conflict_count_map;
	delete pathTraces;
}


vector<double> ATFMSectorDomain::getPerformance(){
	return matrix1d(sectors->size(),-conflict_count);
}

/**
* Loads and capacities are [sector][type]
*/
double ATFMSectorDomain::G(vector<vector<int> > loads, vector<vector<int> > capacities){
	double global=0;
	for (unsigned int i=0; i<loads.size(); i++){
		for (unsigned int j=0; j<loads[i].size(); j++){
			double overcap = loads[i][j] - capacities[i][j];
			global -= overcap*overcap; // worse with higher concentration of planes!
		}
	}
	return global;
}

/**
* Go through all the sectors and return loads, format [sector][type]
*/
vector<vector<int> > ATFMSectorDomain::getLoads(){
	vector<vector<int> > allloads = vector<vector<int> >(sectors->size());
	for (unsigned int i=0; i<sectors->size(); i++){
		allloads[i] = sectors->at(i).getLoad();
	}
	return allloads;
}

vector<double> ATFMSectorDomain::getRewards(){
	// LINEAR REWARD
	return matrix1d(sectors->size(),-conflict_count); // linear reward


	// QUADRATIC REWARD
	/*int conflict_sum = 0;
	for (int i=0; i<conflict_count_map->size(); i++){
	for (int j=0; j<conflict_count_map->at(i).size(); j++){
	int c = conflict_count_map->at(i)[j];
	conflict_sum += c*c;
	}
	}
	return matrix1d(sectors->size(),-conflict_sum);*/
}




matrix2d ATFMSectorDomain::getStates(){
	// States: delay assignments for UAVs that need routing
	matrix2d allStates(n_agents);
	for (int i=0; i<n_agents; i++){
		allStates[i] = matrix1d(n_state_elements,0.0); // reserves space
	}

	for (std::shared_ptr<UAV> &u : UAVs){
		allStates[getSector(u->loc)][u->getDirection()]+=1.0; // Adds the UAV impact on the state
	}

	return allStates;
}

matrix3d ATFMSectorDomain::getTypeStates(){
	matrix3d allStates(n_agents);
	for (int i=0; i<n_agents; i++){
		allStates[i] = matrix2d(n_types);
		for (int j=0; j<n_types; j++){
			allStates[i][j] = matrix1d(n_state_elements,0.0);
		}
	}

	for (std::shared_ptr<UAV> &u: UAVs){
		int a = getSector(u->loc);
		int id = u->type_ID;
		int dir = u->getDirection();
		if (a<0){
			printf("UAV %i at location %f,%f is in an obstacle.!", u->ID,u->loc.x,u->loc.y);
			system("pause");
		}
		allStates[a][id][dir]+=1.0;
	}

	return allStates;
}

unsigned int ATFMSectorDomain::getSector(easymath::XY p){
	// tests membership for sector, given a location
	return membership_map->at(p);
}

//HACK: ONLY GET PATH PLANS OF UAVS just generated
void ATFMSectorDomain::getPathPlans(){
	for (std::shared_ptr<UAV> &u : UAVs){
		u->planDetailPath(); // sets own next waypoint
	}
}

void ATFMSectorDomain::getPathPlans(std::list<std::shared_ptr<UAV> > &new_UAVs){
	for (Sector &s: *sectors){
		s.toward.clear();
	}
	for (std::shared_ptr<UAV> &u : new_UAVs){
		u->planDetailPath(); // sets own next waypoint
		int nextSectorID = u->nextSectorID();
		sectors->at(nextSectorID).toward.push_back(u);
	}
}

void ATFMSectorDomain::simulateStep(matrix2d agent_actions){
	static int calls=0;
	planners->setCostMaps(agent_actions);
	absorbUAVTraffic();
	if (calls%10==0)
		getNewUAVTraffic();
	calls++;
	getPathPlans();
	incrementUAVPath();
	detectConflicts();
	//printf("Conflicts %i\n",conflict_count);
}

void ATFMSectorDomain::incrementUAVPath(){
	for (std::shared_ptr<UAV> &u: UAVs) u->moveTowardNextWaypoint(); // moves toward next waypoint (next in low-level plan)
	// IMPORTANT! At this point in the code, agent states may have changed
}


void ATFMSectorDomain::absorbUAVTraffic(){
	UAVs.remove_if(UAV::at_destination);
	for (Sector &s: *sectors){
		remove_if(s.toward.begin(), s.toward.end(), UAV::at_destination);
	}

}


void ATFMSectorDomain::getNewUAVTraffic(){
	//static int calls = 0;
	//static vector<XY> UAV_targets; // making static targets

	// Generates (with some probability) plane traffic for each sector
	list<std::shared_ptr<UAV> > all_new_UAVs;
	for (Fix f: *fixes){
		list<std::shared_ptr<UAV> > new_UAVs = f.generateTraffic(fixes,pathTraces);
		all_new_UAVs.splice(all_new_UAVs.end(),new_UAVs);

		// obstacle check
		if (new_UAVs.size() && membership_map->at(new_UAVs.front()->loc)<0){
			printf("issue!");
			exit(1);
		}
	}

	getPathPlans(all_new_UAVs);

	UAVs.splice(UAVs.end(),all_new_UAVs);
	//calls++;
}

void ATFMSectorDomain::reset(){
	UAVs.clear();
	planners->reset();
	conflict_count = 0;
	(*conflict_count_map) = ID_grid(planners->obstacle_map->dim1(), planners->obstacle_map->dim2());
}

void ATFMSectorDomain::logStep(int step){
	// log at 0 and 50
	// no logging, for speed
	/*
	if (step==0){
	pathSnapShot(0);
	}
	if (step==50){
	pathSnapShot(50);
	pathTraceSnapshot();
	//exit(1);
	}*/
}

void ATFMSectorDomain::exportLog(std::string fid, double G){
	static int calls = 0;
	PrintOut::toFileMatrix2D(*conflict_count_map,fid+to_string(calls)+".csv");
	calls++;
}

void ATFMSectorDomain::detectConflicts(){
	for (list<std::shared_ptr<UAV> >::iterator u1=UAVs.begin(); u1!=UAVs.end(); u1++){
		for (list<std::shared_ptr<UAV> >::iterator u2=std::next(u1); u2!=UAVs.end(); u2++){

			double d = easymath::distance((*u1)->loc,(*u2)->loc);

			if (d>conflict_thresh) continue; // No conflict!

			conflict_count++;

			int midx = ((int)(*u1)->loc.x+(int)(*u2)->loc.x)/2;
			int midy = ((int)(*u1)->loc.y+(int)(*u2)->loc.y)/2;
			conflict_count_map->at(midx,midy)++;

			if ((*u1)->type_ID==UAV::FAST || (*u2)->type_ID==UAV::FAST){
				conflict_count+=10; // big penalty for high priority ('fast' here)
			}
		}
	}

}

// PATH SNAPSHOT OUTPUT
void ATFMSectorDomain::pathSnapShot(int snapnum){
	matrix2d pathsnaps = matrix2d(2*UAVs.size());
	int ind = 0; // index of path given
	for (std::shared_ptr<UAV> &u:UAVs){
		// pop through queue
		std::queue<XY> wpt_save = u->target_waypoints;
		while (u->target_waypoints.size()){
			pathsnaps[ind].push_back(u->target_waypoints.front().x);
			pathsnaps[ind+1].push_back(u->target_waypoints.front().y);
			u->target_waypoints.pop();
		}
		ind+=2;
		u->target_waypoints = wpt_save;
	}
	PrintOut::toFile2D(pathsnaps,"path-" + to_string(snapnum)+".csv");
}

void ATFMSectorDomain::pathTraceSnapshot(){
	// Exports all path traces
	matrix2d pathsnaps =matrix2d(2*pathTraces->size());
	for (unsigned int i=0, ind=0; i<pathTraces->size(); i++, ind+=2){
		for (unsigned int j=0; j<pathTraces->at(i).size(); j++){
			pathsnaps[ind].push_back(pathTraces->at(i)[j].x);
			pathsnaps[ind+1].push_back(pathTraces->at(i)[j].y);
		}
	}

	PrintOut::toFile2D(pathsnaps,"trace.csv");
}