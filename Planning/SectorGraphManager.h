#pragma once

#include "../../libraries/Planning/GridGraph.h"
#include <map>
// A manager class to handle different instances of grid for different sectors

class SectorGraphManager
{
public:
	typedef std::pair<int,int> Edge;

	SectorGraphManager(matrix2d membership_map, std::vector<Edge> edges);
	~SectorGraphManager(void);

	int getMembership(const easymath::XY &p);
	std::vector<easymath::XY> astar(const easymath::XY &p1, const easymath::XY &p2);

private:
	matrix2d membership_map;
	std::map<int,std::map<int,GridGraph*> > m2graph; // lets you know which A* to access
};