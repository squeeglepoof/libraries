//! Copyright 2016 Carrie Rebhuhn
#include "NeuroEvo.h"
#include <algorithm>
#include <list>
#include <vector>

using std::list;
using std::vector;

NeuroEvoParameters::NeuroEvoParameters(int inputSet, int outputSet) :
    nInput(inputSet), nOutput(outputSet), epsilon(0.1) {
}


void NeuroEvo::updatePolicyValues(double R) {
    // Add together xi values, for averaging
    double xi = 0.1;  // "learning rate" for NE
    double V = (*pop_member_active)->evaluation;
    V = xi*(R - V) + V;
    (*pop_member_active)->evaluation = V;
}

matrix1d NeuroEvo::getAction(matrix1d state) {
    return (*pop_member_active)->predictContinuous(state);
}

matrix1d NeuroEvo::getAction(matrix2d state) {
    matrix1d stateSum(state[0].size(), 0.0);

    // state[type][state_element] -- specifies combination for state
    for (size_t i = 0; i < state.size(); i++) {
        for (size_t j = 0; j < state[i].size(); j++) {
            stateSum[j] += state[i][j];
        }
    }

    return getAction(stateSum);
}

NeuroEvo::NeuroEvo(NeuroEvoParameters* neuroEvoParamsSet) {
    params = neuroEvoParamsSet;
    for (int i = 0; i < params->popSize; i++) {
        NeuralNet* nn = new NeuralNet(params->nInput,
            params->nHidden, params->nOutput);
        population.push_back(nn);
    }
    pop_member_active = population.begin();
}

void NeuroEvo::deletePopulation() {
    while (!population.empty()) {
        delete population.back();
        population.pop_back();
    }
}

NeuroEvo::~NeuroEvo(void) {
    deletePopulation();
}

bool NeuroEvo::selectNewMember() {
    ++pop_member_active;
    if (pop_member_active == population.end()) {
        pop_member_active = population.begin();
        return false;
    } else {
        return true;
    }
}

void NeuroEvo::generateNewMembers() {
    // Mutate existing members to generate more
    list<NeuralNet*>::iterator popMember = population.begin();
    for (int i = 0; i < params->popSize; i++) {  // add k new members
        // commented out so that you take parent's evaluation
        // (*popMember)->evaluation = 0.0;
        // dereference pointer AND iterator
        NeuralNet* m = new NeuralNet(**popMember);
        m->mutate();
        population.push_back(m);
        ++popMember;
    }
}

double NeuroEvo::getBestMemberVal() {
    // Find the HIGHEST FITNESS value of any neural network
    double highest = population.front()->evaluation;
    for (NeuralNet* p : population) {
        if (highest < p->evaluation) highest = p->evaluation;
    }
    return highest;
}

void random_shuffle(list<NeuralNet*> *L) {
    vector<NeuralNet*> tmp(L->begin(), L->end());
    random_shuffle(tmp.begin(), tmp.end());
    copy(tmp.begin(), tmp.end(), L->begin());
}

void NeuroEvo::selectSurvivors() {
    // Select neural networks with the HIGHEST FITNESS
    population.sort(NNCompare);  // Sort by the highest fitness
    int nExtraNN = population.size() - params->popSize;
    for (int i = 0; i < nExtraNN; i++) {  // Remove the extra
        delete population.back();
        population.pop_back();
    }
    random_shuffle(&population);

    pop_member_active = population.begin();
}


void NeuroEvo::deepCopy(const NeuroEvo &NE) {
    // Creates new pointer addresses for the neural nets
    params = NE.params;

    deletePopulation();
    for (NeuralNet* p : NE.population) {
        population.push_back(new NeuralNet(*p));
    }
}
