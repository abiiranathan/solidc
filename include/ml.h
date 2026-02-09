/**
 * @file ml.h
 * @brief Simple Machine Learning library using 4x4 Matrices and Vectors.
 *
 * Demonstrates a real-world use case for the vector and matrix libraries.
 * Implements a simple Feed-Forward Neural Network (MLP) with Sigmoid activation.
 *
 * Limitations:
 * - Fixed layer width of 4 neurons (due to Mat4/Vec4 usage).
 * - Fixed network depth (configurable up to MAX_LAYERS).
 */

#ifndef ML_H
#define ML_H

#ifdef __cplusplus
extern "C" {
#endif

#include "matrix.h"
#include "vec.h"

// Max layers for the fixed-size network
#define ML_MAX_LAYERS 8

/**
 * @struct ML_Layer
 * @brief Represents a single fully connected layer 4 inputs -> 4 outputs.
 */
typedef struct ML_Layer {
    Mat4 weights;     // Weights matrix (4x4)
    Vec4 bias;        // Bias vector (4)
    
    // Cache for backpropagation
    Vec4 input;       // Input to this layer
    Vec4 z;           // Pre-activation output (Wx + b)
    Vec4 output;      // Post-activation output (Sigmoid(z))
    
    // Gradients
    Mat4 d_weights;
    Vec4 d_bias;
} ML_Layer;

/**
 * @struct ML_Network
 * @brief A neural network composed of multiple layers.
 */
typedef struct ML_Network {
    ML_Layer layers[ML_MAX_LAYERS];
    int layer_count;
    float learning_rate;
} ML_Network;

/**
 * @brief Initialize a new network.
 * @param net Pointer to the network struct.
 * @param num_layers Number of layers to create.
 * @param learning_rate Learning rate for gradient descent.
 */
void ml_init(ML_Network* net, int num_layers, float learning_rate);

/**
 * @brief Perform forward propagation.
 * @param net Pointer to the network.
 * @param input Input vector (4 floats).
 * @return Output vector (4 floats).
 */
Vec4 ml_forward(ML_Network* net, Vec4 input);

/**
 * @brief Train the network on a single sample.
 * @param net Pointer to the network.
 * @param input Input vector.
 * @param target Target output vector.
 * @return MSE Loss for this sample.
 */
float ml_train_step(ML_Network* net, Vec4 input, Vec4 target);

#ifdef __cplusplus
}
#endif

#endif // ML_H
