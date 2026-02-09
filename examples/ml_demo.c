#include "../include/ml.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main() {
    srand(time(NULL));
    
    printf("=== Simple Machine Learning Library Demo (XOR) ===\n");
    
    // 1. Initialize Network
    ML_Network net;
    ml_init(&net, 2, 0.5f); // 2 layers, learning rate 0.5
    
    printf("Network Initialized. Layers: %d, LR: %.2f\n", net.layer_count, net.learning_rate);
    
    // 2. Training Data (XOR)
    Vec4 inputs[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 0.0f}
    };
    
    Vec4 targets[4] = {
        {0.0f, 0.0f, 0.0f, 0.0f}, // 0 ^ 0 = 0
        {1.0f, 0.0f, 0.0f, 0.0f}, // 0 ^ 1 = 1
        {1.0f, 0.0f, 0.0f, 0.0f}, // 1 ^ 0 = 1
        {0.0f, 0.0f, 0.0f, 0.0f}  // 1 ^ 1 = 0
    };
    
    // 3. Training Loop
    int epochs = 10000;
    printf("Training for %d epochs...\n", epochs);
    
    for(int i=0; i<epochs; i++) {
        float total_loss = 0.0f;
        for(int j=0; j<4; j++) {
            // Pick random sample or sequential
            int idx = j;
            total_loss += ml_train_step(&net, inputs[idx], targets[idx]);
        }
        
        if (i % 1000 == 0) {
            printf("Epoch %d, Loss: %.5f\n", i, total_loss / 4.0f);
        }
    }
    
    // 4. Testing
    printf("\n=== Testing Predictions ===\n");
    for(int i=0; i<4; i++) {
        Vec4 out = ml_forward(&net, inputs[i]);
        printf("Input: (%.0f, %.0f) -> Target: %.0f, Prediction: %.4f\n", 
               inputs[i].x, inputs[i].y, targets[i].x, out.x);
    }
    
    return 0;
}
