#include "../include/ml.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* --- Helper Functions --- */

static float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

static float sigmoid_derivative(float sig) { return sig * (1.0f - sig); }

static Vec4 vec4_sigmoid(Vec4 v) { return (Vec4){sigmoid(v.x), sigmoid(v.y), sigmoid(v.z), sigmoid(v.w)}; }

static Vec4 vec4_sigmoid_derivative(Vec4 v) {
    return (Vec4){sigmoid_derivative(v.x), sigmoid_derivative(v.y), sigmoid_derivative(v.z), sigmoid_derivative(v.w)};
}

static float random_float() {
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;  // -1 to 1
}

static Mat4 random_mat4() {
    Mat4 m;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m.m[i][j] = random_float();
        }
    }
    return m;
}

static Vec4 random_vec4() { return (Vec4){random_float(), random_float(), random_float(), random_float()}; }

/* --- ML Implementation --- */

void ml_init(ML_Network* net, int num_layers, float learning_rate) {
    if (num_layers > ML_MAX_LAYERS) num_layers = ML_MAX_LAYERS;
    net->layer_count = num_layers;
    net->learning_rate = learning_rate;

    for (int i = 0; i < num_layers; i++) {
        net->layers[i].weights = random_mat4();
        net->layers[i].bias = random_vec4();
    }
}

Vec4 ml_forward(ML_Network* net, Vec4 input) {
    Vec4 current_input = input;

    for (int i = 0; i < net->layer_count; i++) {
        ML_Layer* layer = &net->layers[i];

        layer->input = current_input;

        // Linear: z = W * x + b
        Vec4 wx = mat4_mul_vec4(layer->weights, current_input);

        // Add bias (component-wise)
        // Need to load to SIMD for addition
        SimdVec4 s_wx = vec4_load(wx);
        SimdVec4 s_bias = vec4_load(layer->bias);
        SimdVec4 s_z = vec4_add(s_wx, s_bias);

        layer->z = vec4_store(s_z);

        // Activation
        layer->output = vec4_sigmoid(layer->z);
        current_input = layer->output;
    }

    return current_input;
}

float ml_train_step(ML_Network* net, Vec4 input, Vec4 target) {
    // 1. Forward Pass
    Vec4 prediction = ml_forward(net, input);

    // 2. Compute Loss (MSE) and Output Gradient
    // Loss = 0.5 * (target - output)^2
    // dLoss/dOutput = (output - target)
    SimdVec4 s_target = vec4_load(target);
    SimdVec4 s_output = vec4_load(prediction);
    SimdVec4 s_error = vec4_sub(s_output, s_target);  // Gradient of Loss w.r.t Output

    float mse = vec4_length_sq(s_error) * 0.5f;

    // 3. Backpropagation
    // We propagate 'delta' backwards.
    // Delta = dLoss/dZ = dLoss/dOutput * dOutput/dZ
    // dOutput/dZ = sigmoid'(output)

    Vec4 current_delta_vec = vec4_store(s_error);  // dL/dY

    for (int i = net->layer_count - 1; i >= 0; i--) {
        ML_Layer* layer = &net->layers[i];

        // Calculate Activation Derivative
        Vec4 d_act = vec4_sigmoid_derivative(layer->output);
        SimdVec4 s_d_act = vec4_load(d_act);
        SimdVec4 s_curr_delta = vec4_load(current_delta_vec);

        // Element-wise multiply: delta = error * sigmoid'
        SimdVec4 s_delta = vec4_scale(s_curr_delta, s_d_act);
        Vec4 delta = vec4_store(s_delta);

        // Gradients
        // dW = delta * input^T
        // db = delta

        // Update Weights: W = W - lr * dW
        // dW[r][c] = delta[r] * input[c]
        // This is an outer product: delta (col) * input (row)

        // We need to construct dW matrix.
        Mat4 dW;
        // Col 0 = input.x * delta
        // Col 1 = input.y * delta
        // ...
        // Wait. dW_ij = delta_i * input_j.
        // In column-major M[col][row], this is M[j][i] = delta[i] * input[j].
        // So Col j contains (delta_0 * input_j, delta_1 * input_j, ...) = delta * input_j.

        SimdVec4 s_delta_simd = vec4_load(delta);

        dW.cols[0] = vec4_mul(s_delta_simd, layer->input.x).v;
        dW.cols[1] = vec4_mul(s_delta_simd, layer->input.y).v;
        dW.cols[2] = vec4_mul(s_delta_simd, layer->input.z).v;
        dW.cols[3] = vec4_mul(s_delta_simd, layer->input.w).v;

        // Store Gradient (optional if we want batching, but we do SGD here)
        layer->d_weights = dW;
        layer->d_bias = delta;

        // Propagate error to previous layer
        // PrevError = W^T * Delta
        if (i > 0) {
            Mat4 W_T = mat4_transpose(layer->weights);
            // mat4_mul_vec4 computes M * v.
            // We need W^T * delta.
            current_delta_vec = mat4_mul_vec4(W_T, delta);
        }

        // Apply Updates
        // W = W - lr * dW
        Mat4 step_W = mat4_scalar_mul(dW, net->learning_rate);
        layer->weights = mat4_sub(layer->weights, step_W);

        // b = b - lr * db
        SimdVec4 s_db = vec4_load(delta);
        SimdVec4 s_step_b = vec4_mul(s_db, net->learning_rate);
        SimdVec4 s_new_b = vec4_sub(vec4_load(layer->bias), s_step_b);
        layer->bias = vec4_store(s_new_b);
    }

    return mse;
}
