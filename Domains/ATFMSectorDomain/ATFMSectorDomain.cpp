#include "ATFMSectorDomain.h"

using namespace std;
using namespace easymath;

UAV::UAV(XY start_loc, XY end_loc,std::vector<std::vector<XY> > *pathTraces, UAVType t):
	loc(start_loc), end_loc(end_loc), ID(pathTraces->size()), pathTraces(pathTraces), type_ID(t)
{
	pathTraces->push_back(vector<XY>(1,loc)); // new path trace created for each UAV,starting at location

	switch(t){
	case SLOW:
		speed = 1;
		break;
	case FAST:
		// substitute: high priority
		speed = 1;
		//speed = 2;
		break;
	default:{
		printf("no speed found!!");
		system("pause");
		break;
			}
	}

};

void UAV::pathPlan(AStar_easy* Astar_highlevel, grid_lookup &m2astar, barrier_grid*obstacle_map,
				   ID_grid* membership_map, vector<Sector>* sectors)
{
	int memstart = membership_map->at(loc.x,loc.y);
	int memend = membership_map->at(end_loc.x,end_loc.y);
	list<AStar_easy::vertex> high_path = Astar_highlevel->search(memstart,memend);

	if (high_path_prev != high_path){ // Check if any course change necessary
		high_path_prev = high_path;


		int memnext;
		if (high_path.size()==1){
			memnext = high_path.front();
		} else {
			memnext = *std::next(high_path.begin());
		}
		XY waypoint;
		if (memnext==memend){
			waypoint = end_loc;
		} else {
			waypoint = sectors->at(memnext).xy;
		}

		// TODO: VERIFY HERE THAT THE HIGH_PATH_BEGIN() IS THE NEXT MEMBER... NOT THE FIRST...
		// target the center of the sector, or the goal if it is reachable
		vector<XY> low_path = m2astar[memstart][memnext]->m.get_solution_path(loc,waypoint);

		if (low_path.empty()) low_path.push_back(loc); // stay in place...

		while (target_waypoints.size()) target_waypoints.pop(); // clear the queue;
		for (int i=0; i<low_path.size(); i++) target_waypoints.push(low_path[i]); // adds waypoints to visit
		target_waypoints.pop(); // remove CURRENT location from target	
	}

	/*
	if(high_path_prev !=high_path){
		high_path_prev = high_path;
		AStar_easy* astar_low;
		if (astar_lowlevel.count(high_path)){
			astar_low = astar_lowlevel[high_path];
		} else{
			// MAKE A NEW LOW-LEVEL ASTAR
			astar_low = new AStar_easy(high_path,obstacle_map,membership_map);
			astar_lowlevel[high_path] = astar_low;
		}
		list<AStar_easy::vertex> low_path= astar_low->search(loc,end_loc);
		while (target_waypoints.size()) target_waypoints.pop();
		for (list<AStar_easy::vertex>::iterator it=low_path.begin(); it!=low_path.end(); it++){
			int x, y;
			astar_low->ind2sub(astar_low->YDIM,*it, x,y);
			target_waypoints.push(XY(x,y)); // adds to list to visit
		}
		target_waypoints.pop(); // removes CURRENT location from target
	}
	*/
}

int UAV::getDirection(){
	// Identifies whether traveling in one of four cardinal directions
	return cardinalDirection(loc-end_loc);
}

Sector::Sector(XY xy): xy(xy)
{
}

Fix::Fix(XY loc, bool deterministic): 
	is_deterministic(deterministic), loc(loc), 
	//p_gen(0.05) // change to 1.0 if traffic controlled elsewhere
	p_gen(int(is_deterministic)*(1.0-0.05)+0.05), // modifies to depend on if deterministic
	dist_thresh(2.0)
{
}

bool Fix::atDestinationFix(const UAV &u){
	return u.target_waypoints.size()					// UAV has planned a trajectory
		&& u.target_waypoints.front()==loc				// UAV wants to go there next
		&& easymath::distance(u.loc,loc)<dist_thresh	// UAV is close enough
		&& u.end_loc==loc;								// This is destination fix
}

std::list<UAV> Fix::generateTraffic(vector<Fix>* allFixes, barrier_grid* obstacle_map,std::vector<std::vector<XY> > *pathTraces){
	// Creates a new UAV in the world
	static int calls = 0;
	std::list<UAV> newTraffic;

	if (is_deterministic && calls%gen_frequency!=0){
		// NO NEW TRAFFIC
		calls++;
		return newTraffic;
	} else {
		// NEW TRAFFIC, PROBABILISTICALLY
		double coin = COIN_FLOOR0;
		if (coin<p_gen){
			XY end_loc;
			do {
				end_loc = allFixes->at(COIN_FLOOR0*allFixes->size()).loc;
			} while (end_loc==loc);
			UAV::UAVType type_id_set = UAV::UAVType(calls%int(UAV::UAVType::NTYPES)); // EVEN TYPE NUMBER
			newTraffic.push_back(UAV(loc,end_loc,pathTraces,type_id_set));
		}

		calls++;
		return newTraffic;
	}
}

void Fix::absorbTraffic(list<UAV>* UAVs){
	list<UAV> cur_UAVs;
	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
		if (atDestinationFix(*u)){
			// don't copy over
		} else {
			if (u->target_waypoints.size() && u->target_waypoints.front()==loc){ // deleted if size==0; drop invalid plans
				u->target_waypoints.pop();
			}
			cur_UAVs.push_back(*u);
		}
	}
	(*UAVs) = cur_UAVs; // copy over
}

ATFMSectorDomain::ATFMSectorDomain(bool deterministic):
	is_deterministic(deterministic)
{
	pathTraces = new vector<vector<XY> >(); // traces the path (reset each epoch)

	// Object creation
	sectors = new vector<Sector>();
	fixes = new vector<Fix>();
	UAVs = new list<UAV>();

	// inherritance elements: constant
	//n_control_elements=4; // 4 outputs for sectors (cost in cardinal directions) (no types)
	n_control_elements=4*UAV::NTYPES;
	n_state_elements=4; // 4 state elements for sectors ( number of planes traveling in cardinal directions)
	n_steps=100; // steps of simulation time
	n_types=UAV::NTYPES;

	// Read in files for sector management
	obstacle_map = new barrier_grid(256,256);
	membership_map = new ID_grid(256,256); //HARDCODED
	load_variable(*obstacle_map,"agent_map/obstacle_map.csv",0.0); // last element specifies height threshold for obstacle
	load_variable(*membership_map,"agent_map/membership_map.csv");

	matrix2d agent_coords = FileManip::readDouble("agent_map/agent_map.csv");
	matrix2d connection_map = FileManip::readDouble("agent_map/connections.csv");
	matrix2d fix_locs = FileManip::readDouble("agent_map/fixes.csv");

	// Add sectors
	agent_locs = vector<XY>(agent_coords.size()); // save this for later Astar recreation
	for (int i=0; i<agent_coords.size(); i++){
		sectors->push_back(Sector(XY(agent_coords[i][1],agent_coords[i][0]))); // SWAPPED; accounts for different coordinates
		agent_locs[i] = XY(agent_coords[i][1],agent_coords[i][0]); // ALSO SWAPPED
	}

	// Adjust the connection map to be the edges
	// preprocess boolean connection map
	// map edges to (sector ind, direction ind)
	for (int i=0; i<connection_map.size(); i++){
		for (int j=0; j<connection_map[i].size(); j++){
			if (connection_map[i][j] && i!=j){
				XY xyi = agent_locs[i];
				XY xyj = agent_locs[j];
				XY dx_dy = xyj-xyi;
				int xydir = cardinalDirection(dx_dy);
				int memj = membership_map->at(xyj); // only care about cost INTO sector
				sector_dir_map[edges.size()] = make_pair(memj,xydir); // add at new index
				edges.push_back(AStar_easy::edge(i,j));
			}
		}
	}

	//weights = vector<double>(edges.size(),1.0); //initialization of weights
	weights = matrix2d(UAV::NTYPES);
	for (int i=0; i<weights.size(); i++){
		weights[i] = matrix1d(edges.size(),1.0);
	}


	// initialize fixes
	for (int i=0; i<fix_locs.size(); i++){
		fixes->push_back(Fix(XY(fix_locs[i][0],fix_locs[i][1]),is_deterministic));
	}

	n_agents = sectors->size(); // number of agents dictated by read in file
	fixed_types=vector<int>(n_agents,0);

	conflict_count = 0; // initialize with no conflicts

	// Initialize Astar object (must re-create this each time weight change
	Astar_highlevel = std::vector<AStar_easy*>(UAV::NTYPES);
	for (int i=0; i<UAV::NTYPES; i++){
		Astar_highlevel[i] = new AStar_easy(agent_locs,edges,weights[i]);
	}

	// Add a different A* for each connection
	for (int i=0; i<connection_map.size(); i++){
		for (int j=0; j<connection_map[i].size(); j++){
			if (connection_map[i][j]>0){
				m2astar[i][j] = new AStar_grid(obstacle_map,membership_map,i,j);
			}
		}
	}

	/*// HACK: PRINT OUT ALL OF THE MASKS!
	for (grid_lookup::iterator it=m2astar.begin(); it!=m2astar.end(); it++){
		for (map<int,AStar_grid*>::iterator inner = it->second.begin(); inner!=it->second.end(); inner++){
			inner->second->m.printMap("masks/", it->first, inner->first);
		}
	}
	system("pause");
	exit(1);
	//end hack*/

	conflict_count_map = new ID_grid(obstacle_map->dim1(), obstacle_map->dim2());
}

ATFMSectorDomain::~ATFMSectorDomain(void)
{
	delete UAVs;
	delete fixes;
	delete sectors;
	delete obstacle_map;
	delete membership_map;

	// Delete all pointers: recreate later...
	for (grid_lookup::iterator it=m2astar.begin(); it!=m2astar.end(); it++){
		for (std::map<int,AStar_grid*>::iterator inner=it->second.begin(); inner!=it->second.end(); inner++){
			delete inner->second;
		}
	}

	//delete m2astar;
	/*for (map<list<AStar_easy::vertex>, AStar_easy* >::iterator it=astar_lowlevel.begin();
		it!=astar_lowlevel.end(); it++){
			delete it->second;
	}*/
	delete conflict_count_map;
	for (int i=0; i<Astar_highlevel.size(); i++){
		delete Astar_highlevel[i];
	}
	delete pathTraces;
}


vector<double> ATFMSectorDomain::getPerformance(){
	return matrix1d(sectors->size(),-conflict_count);
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

	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
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

	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
		int a = getSector(u->loc);
		int id = u->type_ID;
		int dir = u->getDirection();
		if (a<0){
			printf("we have an issue!");
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
	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
		u->pathPlan(Astar_highlevel[u->type_ID],m2astar,obstacle_map,membership_map,sectors); // sets own next waypoint
	}
}

void ATFMSectorDomain::getPathPlans(std::list<UAV> &new_UAVs){
	for (list<UAV>::iterator u=new_UAVs.begin(); u!=new_UAVs.end(); u++){
		u->pathPlan(Astar_highlevel[u->type_ID],m2astar,obstacle_map,membership_map,sectors); // sets own next waypoint
	}
}

void ATFMSectorDomain::simulateStep(matrix2d agent_actions){
	setCostMaps(agent_actions);
	absorbUAVTraffic();
	getNewUAVTraffic();
	getPathPlans();
	incrementUAVPath();
	detectConflicts();
	//printf("Conflicts %i\n",conflict_count);
}

void ATFMSectorDomain::setCostMaps(vector<vector<double> > agent_actions){
	// [next sector][direction of travel] -- current
	// agent_actions  = agent, [type, dir<-- alternating]

	for (int i=0; i<weights.size(); i++){
		for (int j=0; j<UAV::NTYPES; j++){
			weights[j][i] = agent_actions[sector_dir_map[i].first][j*UAV::NTYPES + sector_dir_map[i].second];
		}
		// HACK
		// weights[i] = agent_actions[sector_dir_map[i].first][sector_dir_map[i].second]; // AGENT SETUP // old
		// weights[i] = 1.0; // NO AGENTS
	}

	for (int i=0; i<Astar_highlevel.size(); i++){
		delete Astar_highlevel[i];
		Astar_highlevel[i] = new AStar_easy(agent_locs,edges,weights[i]); // replace existing weights
	}
}

void ATFMSectorDomain::incrementUAVPath(){
	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
		u->moveTowardNextWaypoint(); // moves toward next waypoint (next in low-level plan)
	}
	// IMPORTANT! At this point in the code, agent states may have changed
}

void UAV::moveTowardNextWaypoint(){
	for (int i=0; i<speed; i++){
		if (!target_waypoints.size()) return; // return if no waypoints
		loc = target_waypoints.front();
		pathTraces->at(ID).push_back(loc);
		target_waypoints.pop();
	}
}

void ATFMSectorDomain::absorbUAVTraffic(){
	for (int i=0; i<fixes->size(); i++){
		fixes->at(i).absorbTraffic(UAVs);
	}
}


void ATFMSectorDomain::getNewUAVTraffic(){
	//static vector<XY> UAV_targets; // making static targets

	// Generates (with some probability) plane traffic for each sector
	list<UAV> all_new_UAVs;
	for (int i=0; i<fixes->size(); i++){
		list<UAV> new_UAVs = fixes->at(i).generateTraffic(fixes, obstacle_map,pathTraces);
		all_new_UAVs.splice(all_new_UAVs.end(),new_UAVs);
	}

	getPathPlans(all_new_UAVs);

	UAVs->splice(UAVs->end(),all_new_UAVs);
}

void ATFMSectorDomain::reset(){
	static int calls=0;

	// Drop all UAVs
	//printf("UAVs = %i\n",UAVs->size());
	UAVs->clear();
	// reset weights
	conflict_count = 0;
	//
	//weights = vector<double>(edges.size(),1.0); //initialization of weights
	weights = matrix2d(UAV::NTYPES);
	for (int i=0; i<weights.size(); i++){
		weights[i] = matrix1d(edges.size(), 1.0);
	}

	// re-create high level a*

	for (int i=0; i<Astar_highlevel.size(); i++){
		delete Astar_highlevel[i];
		Astar_highlevel[i] = new AStar_easy(agent_locs,edges,weights[i]);
	}

	// re-create low level a*
	/*for (map<list<AStar_easy::vertex>, AStar_easy* >::iterator it=astar_lowlevel.begin(); it!=astar_lowlevel.end(); it++){
		delete it->second;
	}
	astar_lowlevel.clear();*/

	//PrintOut::toFile(*conflict_count_map,"conflict_map.csv");
	// clear conflict count map
	for (int i=0; i<conflict_count_map->dim1(); i++){
		for (int j=0; j<conflict_count_map->dim2(); j++){
			conflict_count_map->at(i,j) = 0; // set all 0
		}
	}

	// reset the path trace so it doesn't get too big
	// pathTraces->clear();
}

void ATFMSectorDomain::logStep(int step){
	// log at 0 and 50
	if (step==0){
		pathSnapShot(0);
	}
	if (step==50){
		pathSnapShot(50);
		pathTraceSnapshot();
		//exit(1);
	}
}

void ATFMSectorDomain::exportLog(std::string fid, double G){
	static int calls = 0;
	PrintOut::toFileMatrix2D(*conflict_count_map,fid+to_string(calls)+".csv");
	calls++;
}

void ATFMSectorDomain::detectConflicts(){
	double conflict_thresh = 1.0;
	for (list<UAV>::iterator u1=UAVs->begin(); u1!=UAVs->end(); u1++){
		for (list<UAV>::iterator u2=std::next(u1); u2!=UAVs->end(); u2++){
			double d = easymath::distance(u1->loc,u2->loc);

			if (u1!=u2){
				if (d<conflict_thresh){
					conflict_count++;

					int avgx = (u1->loc.x+u2->loc.x)/2;
					int avgy = (u1->loc.y+u2->loc.y)/2;
					conflict_count_map->at(avgx,avgy)++;
					if (u1->type_ID==UAV::FAST || u2->type_ID==UAV::FAST){
						conflict_count+=10; // big penalty for high priority ('fast' here)
					}
				}
			}

		}
	}
}

// PATH SNAPSHOT OUTPUT
void ATFMSectorDomain::pathSnapShot(int snapnum){
	matrix2d pathsnaps = matrix2d(2*UAVs->size());
	int ind = 0; // index of path given
	for (list<UAV>::iterator u=UAVs->begin(); u!=UAVs->end(); u++){
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
	for (int i=0, ind=0; i<pathTraces->size(); i++, ind+=2){
		for (int j=0; j<pathTraces->at(i).size(); j++){
			pathsnaps[ind].push_back(pathTraces->at(i)[j].x);
			pathsnaps[ind+1].push_back(pathTraces->at(i)[j].y);
		}
	}

	PrintOut::toFile2D(pathsnaps,"trace.csv");
}