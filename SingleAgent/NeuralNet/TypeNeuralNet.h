#pragma once

#include "NeuralNet.h"

class TypeNeuralNet: public NeuralNet
{
public:
	TypeNeuralNet(int input, int hidden, int output, matrix3d preprocess_weights):
		NeuralNet(input, hidden, output), preprocess_weights(preprocess_weights){
			for (matrix2d &l1 : preprocess_weights){
				for (matrix1d &l2: l1){
					for (double &l3: l2){
						double fan_in = double(preprocess_weights.size());
						l3 = randSetFanIn(fan_in);
					}
				}
			}
	}

	matrix3d preprocess_weights; // [t][s][t']

	void mutate(){
		NeuralNet::mutate();

		// now mutate the preprocess weights
		for (matrix2d &l1 : preprocess_weights){
			for (matrix1d &l2 : l1){
				for (double &l3 : l2){
					double fan_in = double(preprocess_weights.size());
					l3 += randAddFanIn(fan_in);
				}
			}
		}
	}

	~TypeNeuralNet(void);
};