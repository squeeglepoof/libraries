#include "UTMDomainAbstract.h"


UTMDomainAbstract::UTMDomainAbstract()
{

	// Hardcoding of base constants
	n_state_elements=4; // 4 state elements for sectors ( number of planes traveling in cardinal directions)
	n_control_elements=n_state_elements*UAV::NTYPES;
	n_steps=100; // steps of simulation time
	n_types=UAV::NTYPES;

	// Object creation
	sectors = new vector<Sector>();
	fixes = new vector<Fix>();


	// Read in files for sector management
	Load::load_variable(agent_locs, "agent_map/agent_map.csv");
	Load::load_variable(edges, "agent_map/edges.csv");

	// Add sectors
	for (unsigned int i=0; i<agent_locs.size(); i++){
		sectors->push_back(Sector(agent_locs[i],i, agent_locs.size()));
	}

	n_agents = agent_locs.size(); // number of agents dictated by read in file
	fixed_types=vector<int>(n_agents,0);

	conflict_count = 0; // initialize with no conflicts
	conflict_minus_downstream = matrix1d(n_agents,0.0);
	conflict_minus_touched = matrix1d(n_agents, 0.0);
	conflict_node_average = matrix1d(n_agents,0.0);
	conflict_random_reallocation = matrix1d(n_agents,0.0);

	_reward_mode = GLOBAL;

	for (int i=0; i<sectors->size(); i++){
		sectors->at(i).conflicts = vector<int>(4,0);
	}
	
	// Planning
	planners = new AStarManager(UAV::NTYPES, edges, agent_locs); //NOTE: MAY NOT HAVE TO MAKE A DIFFERENT ONE FOR ABSTRACTION???

	// initialize fixes
	for (unsigned int i=0; i<sectors->size(); i++){
		fixes->push_back(Fix(sectors->at(i).xy,i,planners));
	}
	
	sector_capacity = matrix2d(n_agents,matrix1d(UAV::NTYPES,2.0) ); // arbitrary/flat sector capacity assignment

	for_each_pairing(agent_locs, agent_locs, connection_time, manhattan_dist); // gets the connection time: note this is also calculated for unconnected links
}


UTMDomainAbstract::~UTMDomainAbstract(void)
{
	delete fixes;
	delete sectors;
	delete planners;
}

double UTMDomainAbstract::getGlobalReward(){
	return -conflict_count; // REPLACE THIS LATER
}

matrix1d UTMDomainAbstract::getPerformance(){
	return matrix1d(n_agents,getGlobalReward());
}

matrix1d UTMDomainAbstract::getDifferenceReward(){
	// REMOVE THE AGENT FROM THE SYSTEM
	
	matrix1d D(n_agents,0.0);
	double G_reg = conflict_count;
	
	// METHOD 1: INFINITE LINK COSTS (*resim) // NOT FUNCTIONAL YET
	// METHOD 2: STATIC LINK COSTS (*resim) // NOT FUNCTIONAL YET
	// METHOD 3: HAND-CODED LINK COSTS (*resim) // NOT FUNCTIONAL YET
	// METHOD 4: DOWNSTREAM EFFECTS REMOVED
	for (int i=0; i<n_agents; i++){
//		D[i] = G_reg - conflict_minus_downstream[i];

		// METHOD 5: UPSTREAM AND DOWNSTREAM EFFECTS REMOVED
//		D[i] = G_reg - conflict_minus_touched[i]; // NOT FUNCTIONAL YET

		// METHOD 6: RANDOM TRAFFIC REALLOCATION
		D[i] = -(G_reg - conflict_random_reallocation[i]);
	
		// METHOD 7: CONFLICTS AVERAGED OVER THE NODE'S HISTORY
//		D[i] = G_reg - conflict_node_average[i]; // NOT FUNCTIONAL YET
		
	}
	return D;

}

matrix1d UTMDomainAbstract::getLocalReward(){
	matrix1d L(n_agents,0.0);
	for (int i=0; i<sectors->size(); i++){
		for (int j=0; j<UAV::NTYPES; j++){
			L[i] += sectors->at(i).conflicts[j];
		}
	}
	return L;
}

matrix1d UTMDomainAbstract::getRewards(){
	// Calculate loads
	if (_reward_mode==GLOBAL){
		return matrix1d(n_agents, getGlobalReward());
	} else if (_reward_mode==DIFFERENCE){
		return getDifferenceReward();
	} else {
		printf("No reward type set.");
		system("pause");
		exit(1);
	}
}

void UTMDomainAbstract::incrementUAVPath(){
	// in abstraction mode, move to next center of target
	for (std::shared_ptr<UAV> &u: UAVs){
		if (u->pathChanged){
			u->t = connection_time[u->curSectorID()][u->nextSectorID()];
		} else if (u->t <=0){
			u->high_path_prev.pop_front();
			u->loc = sectors->at(u->high_path_prev.front()).xy;
		} else {
			u->t--;
		}
	}
}


matrix2d UTMDomainAbstract::getStates(){
	// States: delay assignments for UAVs that need routing
	matrix2d allStates(n_agents);
	for (int i=0; i<n_agents; i++){
		allStates[i] = matrix1d(n_state_elements,0.0); // reserves space
	}

	/* "NORMAL POLARITY" state
	for (std::shared_ptr<UAV> &u : UAVs){
		allStates[getSector(u->loc)][u->getDirection()]+=1.0; // Adds the UAV impact on the state
	}
	*/

	// REVERSED POLARITY STATE
	/*for (std::shared_ptr<UAV> &u : UAVs){
		// COUNT THE UAV ONLY IF IT HAS A NEXT SECTOR IT'S GOING TO
		if (u->nextSectorID()==u->curSectorID()) continue;
		else allStates[u->nextSectorID()][u->getDirection()] += 1.0;
	}*/

	// CONGESTION STATE
	vector<int> sector_congestion_count(n_agents,0);
	for (std::shared_ptr<UAV> &u : UAVs){
		sector_congestion_count[u->curSectorID()]++;
	}
	for (int i=0; i<n_agents; i++){
		for (int j=0; j<planners->edges.size(); j++){
			int conn;
			if (planners->edges[j].first==i){
				conn = planners->edges[j].second;
			} else if (planners->edges[j].second==i){
				conn = planners->edges[j].first;
			} else continue;

			allStates[i][cardinalDirection(sectors->at(i).xy-sectors->at(conn).xy)] += sector_congestion_count[conn];
		}
	}

	return allStates;
}


void UTMDomainAbstract::simulateStep(matrix2d agent_actions){
	static int calls=0;
	planners->setCostMaps(agent_actions);
	absorbUAVTraffic();
	if (Fix::_traffic_mode==Fix::PROBABILISTIC || calls%10==0)
		getNewUAVTraffic();
	calls++;
	getPathPlans();
	incrementUAVPath();
	detectConflicts();
	//printf("Conflicts %i\n",conflict_count);
}


void UTMDomainAbstract::logStep(int step){
}

matrix3d UTMDomainAbstract::getTypeStates(){
	matrix3d allStates(n_agents);
	for (int i=0; i<n_agents; i++){
		allStates[i] = matrix2d(n_types);
		for (int j=0; j<n_types; j++){
			allStates[i][j] = matrix1d(n_state_elements,0.0);
		}
	}

	for (std::shared_ptr<UAV> &u: UAVs){
		int a = u->curSectorID();
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

void UTMDomainAbstract::detectConflicts(){
	// count the over capacity here

	// Calculate the amount OVER or UNDER the given capacity
	matrix2d cap = sector_capacity;

	for (std::shared_ptr<UAV> &u: UAVs){
		double c = cap[u->curSectorID()][u->type_ID]--;
		if (c<0){
			conflict_count++;
			for  (int j=0; j<UAV::NTYPES; j++){
				sectors->at(u->curSectorID()).conflicts[j]++;
			}
			for (int i=0; i<sectors->size(); i++){
				if (u->sectors_touched.find(i)==u->sectors_touched.end()){
					// Not touched, still counted
					conflict_minus_downstream[i]++;
				}
			}
		}
	}

	for (int i=0; i<n_agents; i++){
		sectors->at(i).steps++; // steps of conflict accounting (for average counterfactual)
		// random reallocation

		matrix2d cap_i = cap;
		matrix1d occ_i = cap[i];
		cap_i[i] = matrix1d(cap_i[0].size(),0);
		
		for (int j=0; j<UAV::NTYPES; j++){
			while (occ_i[j]<0){ // ONLY TAKE OUT THE OVER CAPACITY--ASSUME PERFECT ROUTING?
				// add back up to capacity
				occ_i[j]++;
				
				int alt;
				do {
					alt = rand()%n_agents;
				} while(alt==i);
				cap_i[alt][j]--;
			}
		}
		for (int j=0; j<cap_i.size(); j++){
			for (int k=0; k<cap_i[j].size(); k++){
				if (cap_i[j][k]<0){
					conflict_random_reallocation[i]++;
				}
			}
		}
	}
}

void UTMDomainAbstract::getPathPlans(){
	
	for (std::shared_ptr<UAV> &u : UAVs){
		u->planAbstractPath();
	}
}

void UTMDomainAbstract::getPathPlans(std::list<std::shared_ptr<UAV> > &new_UAVs){

	for (std::shared_ptr<UAV> &u : new_UAVs){
		u->planAbstractPath(); // sets own next waypoint
	}
}

void UTMDomainAbstract::exportLog(std::string fid, double G){
}

void UTMDomainAbstract::reset(){
	UAVs.clear();
	planners->reset();
	for (int i=0; i<sectors->size(); i++){
		sectors->at(i).conflicts = vector<int>(4,0);
	}
	conflict_count = 0; // initialize with no conflicts
	conflict_minus_downstream = matrix1d(n_agents,0.0);
	conflict_minus_touched = matrix1d(n_agents, 0.0);
	conflict_node_average = matrix1d(n_agents,0.0);
	conflict_random_reallocation = matrix1d(n_agents,0.0);
}

void UTMDomainAbstract::absorbUAVTraffic(){
	UAVs.remove_if(at_destination);
}


void UTMDomainAbstract::getNewUAVTraffic(){
	//static int calls = 0;
	//static vector<XY> UAV_targets; // making static targets

	// Generates (with some probability) plane traffic for each sector
	list<std::shared_ptr<UAV> > all_new_UAVs;
	for (Fix f: *fixes){
		list<std::shared_ptr<UAV> > new_UAVs = f.generateTraffic(fixes);
		all_new_UAVs.splice(all_new_UAVs.end(),new_UAVs);
	}

	UAVs.splice(UAVs.end(),all_new_UAVs);
	//calls++;
}