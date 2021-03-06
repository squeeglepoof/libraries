//! Copyright 2016 Carrie Rebhuhn
#ifndef SINGLEAGENT_NEUROEVO_NEUROEVOTYPECROSSWEIGHTED_H_
#define SINGLEAGENT_NEUROEVO_NEUROEVOTYPECROSSWEIGHTED_H_

#include <list>
#include "NeuroEvo.h"
#include "../../SingleAgent/NeuralNet/TypeNeuralNet.h"

class NeuroEvoTypeCrossweighted : public NeuroEvo {
 public:
    NeuroEvoTypeCrossweighted(NeuroEvoParameters* NE_params, int n_types, int n_state_elements) :
        NeuroEvo(NE_params), n_types(n_types), n_state_elements(n_state_elements) {
        matrix3d preprocess_weights = easymath::zeros(n_types, n_state_elements, n_types);
        deletePopulation();

        // Neural network parameters
        // NOTE: this is after the "preprocess" step
        int INPUT = n_types * n_state_elements;
        int OUTPUT = params->nOutput;
        int HIDDEN = params->nHidden;
        for (int i = 0; i < NE_params->popSize; i++) {
            population.push_back(new TypeNeuralNet(INPUT, HIDDEN, OUTPUT, preprocess_weights));
        }
        pop_member_active = population.begin();
        params->nInput = INPUT;
    }
    int n_types, n_state_elements;

    ~NeuroEvoTypeCrossweighted() {}

    virtual void generateNewMembers() {
        // Mutate existing members to generate more
        std::list<NeuralNet*>::iterator popMember = population.begin();
        for (int i = 0; i < params->popSize; i++) {  // add k new members
            // (*popMember)->evaluation = 0.0;  // commented out so that you take parent's evaluation
            TypeNeuralNet* m = new TypeNeuralNet(*reinterpret_cast<TypeNeuralNet*>(*popMember));  // dereference pointer AND iterator
            m->mutate();
            population.push_back(m);
            ++popMember;
        }
    }

    using NeuroEvo::getAction;  // so that the overloaded base function is seen

    matrix1d getAction(matrix2d state) {
        matrix1d preprocessed_state(n_state_elements*n_types, 0.0);
        int ind = 0;  // index for state;
        // freaky for loop makes math right
        for (int s = 0; s < n_state_elements; s++) {
            for (size_t t_prime = 0; t_prime < (reinterpret_cast<TypeNeuralNet*>(*pop_member_active))->preprocess_weights[0][0].size(); t_prime++) {
                double node_sum = 0.0;
                for (int t = 0; t < n_types; t++) {
                    node_sum += state[t][s] * (reinterpret_cast<TypeNeuralNet*>(*pop_member_active))->preprocess_weights[t][s][t_prime];
                }
                preprocessed_state[ind++] = node_sum;
            }
        }
        return getAction(preprocessed_state);
    }
};
#endif  // SINGLEAGENT_NEUROEVO_NEUROEVOTYPECROSSWEIGHTED_H_
// Copyright 2016 Carrie Rebhuhn
