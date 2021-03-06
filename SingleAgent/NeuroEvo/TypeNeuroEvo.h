// Copyright 2016 Carrie Rebhuhn
#ifndef SINGLEAGENT_NEUROEVO_TYPENEUROEVO_H_
#define SINGLEAGENT_NEUROEVO_TYPENEUROEVO_H_

#include <algorithm>
#include <functional>
#include <vector>

#include "NeuroEvo.h"
#include "../../Math/easymath.h"
#include "../IAgent.h"

class TypeNeuroEvo : public IAgent {
 public:
    TypeNeuroEvo(void);
    TypeNeuroEvo(NeuroEvoParameters* NEParams, int nTypes) :
        NETypes(std::vector<NeuroEvo*>(nTypes)),
        xi(matrix1d(nTypes, 0.0)) {
        for (NeuroEvo* ne : NETypes) {
            ne = new NeuroEvo(NEParams);
        }
    }
    void deepCopyNETypes(const std::vector<NeuroEvo*> &NETypesSet) {
        // Creates new pointer addresses for the neuro evo instances
        // Calls functions inside neuro evo to create new neural net pointers
        // deletes original pointers

        for (NeuroEvo* ne : NETypes) {
            delete ne;
        }

        NETypes = std::vector<NeuroEvo*>(NETypesSet.size());
        for (size_t i = 0; i < NETypesSet.size(); i++) {
            NETypes[i] = new NeuroEvo();
            NETypes[i]->deepCopy(*NETypesSet[i]);
            NETypes[i]->pop_member_active = NETypes[i]->population.begin();
        }
    }

    // the set of neuro-evo instances for each type in the system
    std::vector<NeuroEvo*> NETypes;



    virtual void generateNewMembers() {
        for (NeuroEvo* ne : NETypes) {
            ne->generateNewMembers();
        }
    }
    bool selectNewMemberAll() {
        // note; only checks the last
        bool selected = false;
        for (NeuroEvo* ne : NETypes) {
            selected = ne->selectNewMember();
        }
        return selected;
    }

    matrix1d getBestMemberValAll() {
        matrix1d memberVals = matrix1d(NETypes.size());
        for (size_t i = 0; i < NETypes.size(); i++) {
            memberVals[i] = NETypes[i]->getBestMemberVal();
        }
        return memberVals;
    }
    void selectSurvivorsAll() {
        for (NeuroEvo* ne : NETypes) {
            ne->selectSurvivors();
        }
    }

    matrix1d getAction(matrix1d state) {
        printf("GetAction being called in multimind setting. Need ");
        printf("neighbor_type identification, or else this will not work. ");
        printf("Debug before continuing.");
        std::system("pause");
        exit(10);
        return matrix1d();
    }


    matrix1d getAction(matrix1d state, int neighbor_type) {
        xi[neighbor_type]++;
        return NETypes[neighbor_type]->getAction(state);
    }


    matrix1d getAction(matrix2d state) {
        // vote among all TYPES for an action
        matrix1d action_sum = getAction(state[0], 0);

        // starts at 1: initialized by 0
        for (size_t j = 1; j < state.size(); j++) {
            // specifies which NN to use
            matrix1d action_sum_temp = getAction(state[j], j);
            for (size_t k = 0; k < action_sum.size(); k++) {
                action_sum[k] += action_sum_temp[k];
            }
        }
        for (double &a : action_sum) {
            // normalize (magnitude unbounded for voting)
            a /= static_cast<double>(state.size());
        }
        return action_sum;
    }

    // eligibility trace: count of how many times each neural net used in run
    matrix1d xi;

    void updatePolicyValues(double R) {
        // Add together xi values, for averaging
        double sumXi = easymath::sum(xi);
        for (size_t i = 0; i < NETypes.size(); i++) {
            // scaled proportional to other member values
            double xi_i = xi[i] / sumXi;
            // get evaluation of active member
            double V = (*NETypes[i]->pop_member_active)->evaluation;
            V = xi_i*(R - V) + V;
            (*NETypes[i]->pop_member_active)->evaluation = V;
        }
        xi = matrix1d(NETypes.size(), 0.0);
    }

    ~TypeNeuroEvo(void);
};
#endif  // SINGLEAGENT_NEUROEVO_TYPENEUROEVO_H_
