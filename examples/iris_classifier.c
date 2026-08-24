/**
 * @file iris_classifier.c
 * @brief A real-world ML workflow: identifying iris species from flower
 *        measurements using a neural network trained on Fisher's classic
 *        Iris dataset (1936, public domain).
 *
 * This example mirrors how applied ML is done in practice:
 *
 *   1. Load real measurements (150 flowers, 4 features, 3 species).
 *   2. Shuffle and split into train (120) / held-out test (30) sets.
 *   3. Standardize features using statistics computed on the TRAIN set
 *      only - never touch the test set before evaluation.
 *   4. Train a 4 -> 16 -> 3 network (tanh hidden layer, softmax output,
 *      cross-entropy loss, mini-batch SGD with momentum).
 *   5. Evaluate honestly on unseen data: accuracy, confusion matrix,
 *      per-class recall.
 *   6. Run inference on brand-new specimens, like a deployed model would.
 *
 * Everything runs on solidc's linear_alg.h primitives - no other ML
 * library involved.
 */

#include "../include/linear_alg.h"
#include "iris_data.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Training hyperparameters */
#define HIDDEN        16
#define EPOCHS        400
#define BATCH_SIZE    16
#define LEARNING_RATE 0.05f
#define MOMENTUM      0.9f

#define TRAIN_COUNT   120 /* remaining 30 are held out for testing */

typedef struct {
    FMat x_train, y_train;
    FMat x_test, y_test;
} Split;

/** Deterministically shuffles sample indices, then splits 120/30. */
static void split_dataset(FMatRng* rng, Split* out) {
    size_t order[IRIS_COUNT];
    for (size_t i = 0; i < IRIS_COUNT; i++) order[i] = i;
    for (size_t i = IRIS_COUNT - 1; i > 0; i--) {
        const size_t j = (size_t)(fmat_rng_uniform(rng) * (float)i);
        const size_t t = order[i];
        order[i] = order[j];
        order[j] = t;
    }

    out->x_train = fmat_create(TRAIN_COUNT, IRIS_FEATURES);
    out->y_train = fmat_create(TRAIN_COUNT, IRIS_CLASSES);
    out->x_test = fmat_create(IRIS_COUNT - TRAIN_COUNT, IRIS_FEATURES);
    out->y_test = fmat_create(IRIS_COUNT - TRAIN_COUNT, IRIS_CLASSES);

    for (size_t i = 0; i < IRIS_COUNT; i++) {
        const bool is_train = i < TRAIN_COUNT;
        FMat* x = is_train ? &out->x_train : &out->x_test;
        FMat* y = is_train ? &out->y_train : &out->y_test;
        const size_t r = is_train ? i : i - TRAIN_COUNT;
        for (size_t c = 0; c < IRIS_FEATURES; c++) {
            fmat_set(x, r, c, iris_features[order[i]][c]);
        }
        fmat_set(y, r, (size_t)iris_labels[order[i]], 1.0f);
    }
}

static void split_destroy(Split* s) {
    fmat_destroy(&s->x_train);
    fmat_destroy(&s->y_train);
    fmat_destroy(&s->x_test);
    fmat_destroy(&s->y_test);
}

/** Per-feature standardization parameters derived ONLY from training data. */
typedef struct {
    FMat mean;    /* 1 x features */
    FMat inv_std; /* 1 x features */
} FeatureScaler;

static void scaler_fit(FeatureScaler* s, const FMat* x_train) {
    s->mean = fmat_mean_cols(x_train);
    s->inv_std = fmat_create(1, x_train->cols);
    for (size_t c = 0; c < x_train->cols; c++) {
        double var = 0.0;
        for (size_t r = 0; r < x_train->rows; r++) {
            const double d = (double)fmat_get(x_train, r, c) - (double)fmat_get(&s->mean, 0, c);
            var += d * d;
        }
        var /= (double)x_train->rows;
        const float std = sqrtf((float)var);
        fmat_set(&s->inv_std, 0, c, std > 1e-8f ? 1.0f / std : 1.0f);
    }
}

/** Returns a standardized copy of src using the fitted scaler. */
static FMat scaler_transform(const FeatureScaler* s, const FMat* src) {
    FMat out = fmat_copy(src);
    for (size_t r = 0; r < out.rows; r++) {
        for (size_t c = 0; c < out.cols; c++) {
            const float v = fmat_get(&out, r, c) - fmat_get(&s->mean, 0, c);
            fmat_set(&out, r, c, v * fmat_get(&s->inv_std, 0, c));
        }
    }
    return out;
}

static void scaler_destroy(FeatureScaler* s) {
    fmat_destroy(&s->mean);
    fmat_destroy(&s->inv_std);
}

/** Copies rows [start, start+count) of m into a fresh matrix. */
static FMat slice_rows(const FMat* m, size_t start, size_t count) {
    FMat r = fmat_create(count, m->cols);
    for (size_t i = 0; i < count; i++) {
        memcpy(r.data + i * m->cols, m->data + (start + i) * m->cols, m->cols * sizeof(float));
    }
    return r;
}

/* ------------------------------------------------------------------ */
/* Neural network: 4 -> HIDDEN -> 3                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    FMat W1, b1;
    FMat W2, b2;
    FMat vW1, vb1, vW2, vb2; /* momentum velocities */
} IrisNet;

static void net_init(IrisNet* n, FMatRng* rng) {
    n->W1 = fmat_create(IRIS_FEATURES, HIDDEN);
    n->b1 = fmat_create(1, HIDDEN);
    n->W2 = fmat_create(HIDDEN, IRIS_CLASSES);
    n->b2 = fmat_create(1, IRIS_CLASSES);
    fmat_xavier_init(&n->W1, IRIS_FEATURES, HIDDEN, rng);
    fmat_xavier_init(&n->W2, HIDDEN, IRIS_CLASSES, rng);
    n->vW1 = fmat_create(IRIS_FEATURES, HIDDEN);
    n->vb1 = fmat_create(1, HIDDEN);
    n->vW2 = fmat_create(HIDDEN, IRIS_CLASSES);
    n->vb2 = fmat_create(1, IRIS_CLASSES);
}

static void net_destroy(IrisNet* n) {
    fmat_destroy(&n->W1);
    fmat_destroy(&n->b1);
    fmat_destroy(&n->W2);
    fmat_destroy(&n->b2);
    fmat_destroy(&n->vW1);
    fmat_destroy(&n->vb1);
    fmat_destroy(&n->vW2);
    fmat_destroy(&n->vb2);
}

/** Forward pass; stores hidden activations for backprop. Returns probabilities. */
static FMat net_forward(const IrisNet* n, const FMat* x, FMat* out_hidden) {
    FMat z1 = fmat_mul(x, &n->W1);
    fmat_add_row_vector(&z1, &n->b1);
    fmat_tanh_ip(&z1);
    *out_hidden = z1; /* move */

    FMat z2 = fmat_mul(out_hidden, &n->W2);
    fmat_add_row_vector(&z2, &n->b2);
    FMat probs = fmat_softmax_rows(&z2);
    fmat_destroy(&z2);
    return probs;
}

/** One mini-batch SGD+momentum step. Returns CE loss before the update. */
static float net_train_step(IrisNet* n, const FMat* x, const FMat* y) {
    const float inv_n = 1.0f / (float)x->rows;

    FMat h, probs = net_forward(n, x, &h);
    const float loss = fmat_cross_entropy(&probs, y);

    // Softmax + cross-entropy shortcut: dL/dZ2 = (p - y) / N
    FMat dz2 = fmat_sub(&probs, y);
    fmat_scale_ip(&dz2, inv_n);

    // Layer-2 gradients
    FMat dw2 = fmat_mul_ta(&h, &dz2);
    FMat db2 = fmat_sum_cols(&dz2);

    // Backpropagate through tanh: tanh'(z) = 1 - tanh(z)^2
    FMat dh = fmat_mul_tb(&dz2, &n->W2);
    FMat hh = fmat_hadamard(&h, &h);
    FMat one = fmat_create(h.rows, h.cols);
    fmat_fill(&one, 1.0f);
    FMat gate = fmat_sub(&one, &hh);
    FMat da1 = fmat_hadamard(&dh, &gate);

    // Layer-1 gradients
    FMat dw1 = fmat_mul_ta(x, &da1);
    FMat db1 = fmat_sum_cols(&da1);

    // Momentum update: v = mom*v + g; W -= lr*v
    FMat tmp = fmat_scale(&n->vW2, MOMENTUM);
    fmat_add_ip(&tmp, &dw2);
    fmat_destroy(&n->vW2);
    n->vW2 = tmp;
    tmp = fmat_scale(&n->vW2, LEARNING_RATE);
    fmat_sub_ip(&n->W2, &tmp);
    fmat_destroy(&tmp);

    tmp = fmat_scale(&n->vb2, MOMENTUM);
    fmat_add_ip(&tmp, &db2);
    fmat_destroy(&n->vb2);
    n->vb2 = tmp;
    tmp = fmat_scale(&n->vb2, LEARNING_RATE);
    fmat_sub_ip(&n->b2, &tmp);
    fmat_destroy(&tmp);

    tmp = fmat_scale(&n->vW1, MOMENTUM);
    fmat_add_ip(&tmp, &dw1);
    fmat_destroy(&n->vW1);
    n->vW1 = tmp;
    tmp = fmat_scale(&n->vW1, LEARNING_RATE);
    fmat_sub_ip(&n->W1, &tmp);
    fmat_destroy(&tmp);

    tmp = fmat_scale(&n->vb1, MOMENTUM);
    fmat_add_ip(&tmp, &db1);
    fmat_destroy(&n->vb1);
    n->vb1 = tmp;
    tmp = fmat_scale(&n->vb1, LEARNING_RATE);
    fmat_sub_ip(&n->b1, &tmp);
    fmat_destroy(&tmp);

    fmat_destroy(&probs);
    fmat_destroy(&dz2);
    fmat_destroy(&dw2);
    fmat_destroy(&db2);
    fmat_destroy(&dh);
    fmat_destroy(&hh);
    fmat_destroy(&one);
    fmat_destroy(&gate);
    fmat_destroy(&da1);
    fmat_destroy(&dw1);
    fmat_destroy(&db1);
    fmat_destroy(&h);
    return loss;
}

/* ------------------------------------------------------------------ */
/* Evaluation                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t matrix[IRIS_CLASSES][IRIS_CLASSES]; /* rows = truth, cols = predicted */
    float accuracy;
    float recall[IRIS_CLASSES];
} Evaluation;

static void net_evaluate(const IrisNet* n, const FeatureScaler* scaler, const FMat* x_test, const FMat* y_test,
                         Evaluation* ev) {
    FMat xs = scaler_transform(scaler, x_test);
    FMat h, probs = net_forward(n, &xs, &h);
    FMat pred = fmat_argmax_rows(&probs);
    FMat truth = fmat_argmax_rows(y_test);

    memset(ev, 0, sizeof(*ev));
    const size_t count = probs.rows;
    for (size_t i = 0; i < count; i++) {
        const size_t p = (size_t)fmat_get(&pred, i, 0);
        const size_t t = (size_t)fmat_get(&truth, i, 0);
        ev->matrix[t][p]++;
        if (p == t) ev->accuracy += 1.0f;
    }
    ev->accuracy /= (float)count;
    for (size_t c = 0; c < IRIS_CLASSES; c++) {
        size_t total = 0;
        for (size_t k = 0; k < IRIS_CLASSES; k++) total += ev->matrix[c][k];
        ev->recall[c] = total ? (float)ev->matrix[c][c] / (float)total : 0.0f;
    }

    fmat_destroy(&xs);
    fmat_destroy(&h);
    fmat_destroy(&probs);
    fmat_destroy(&pred);
    fmat_destroy(&truth);
}

/** Classifies a single new specimen; prints the species probabilities. */
static void predict_specimen(const IrisNet* n, const FeatureScaler* s, const float measurements[IRIS_FEATURES]) {
    FMat raw = fmat_from_array(1, IRIS_FEATURES, measurements);
    FMat xs = scaler_transform(s, &raw);
    FMat h, probs = net_forward(n, &xs, &h);

    printf("  measurements: ");
    for (size_t c = 0; c < IRIS_FEATURES; c++) {
        printf("%s=%.1f%s", iris_feature_names[c], measurements[c], c + 1 < IRIS_FEATURES ? ", " : "");
    }
    printf("\n  prediction:   ");
    for (size_t c = 0; c < IRIS_CLASSES; c++) {
        printf("%s %.1f%%%s", iris_species_names[c], fmat_get(&probs, 0, c) * 100.0f,
               c + 1 < IRIS_CLASSES ? " | " : "");
    }
    printf("\n\n");

    fmat_destroy(&raw);
    fmat_destroy(&xs);
    fmat_destroy(&probs);
    fmat_destroy(&h);
}

int main(void) {
    printf("\nIris species classifier -- solidc linear_alg.h only\n");
    printf("dataset: %d flowers, %d features, %d species (Fisher, 1936)\n\n", IRIS_COUNT, IRIS_FEATURES,
           IRIS_CLASSES);

    // 1. Split into train / held-out test sets.
    FMatRng rng;
    fmat_rng_seed(&rng, 20260824u);
    Split data;
    split_dataset(&rng, &data);
    printf("split: %zu train / %zu test\n", data.x_train.rows, data.x_test.rows);

    // 2. Standardize using TRAIN statistics only.
    FeatureScaler scaler;
    scaler_fit(&scaler, &data.x_train);
    FMat x_train_s = scaler_transform(&scaler, &data.x_train);
    FMat x_test_s = scaler_transform(&scaler, &data.x_test);

    // 3. Train.
    IrisNet net;
    net_init(&net, &rng);

    size_t order[TRAIN_COUNT];
    for (size_t i = 0; i < TRAIN_COUNT; i++) order[i] = i;

    Evaluation eval;
    for (int epoch = 1; epoch <= EPOCHS; epoch++) {
        for (size_t i = TRAIN_COUNT - 1; i > 0; i--) {
            const size_t j = (size_t)(fmat_rng_uniform(&rng) * (float)i);
            const size_t t = order[i];
            order[i] = order[j];
            order[j] = t;
        }

        float loss_sum = 0.0f;
        for (size_t start = 0; start < TRAIN_COUNT; start += BATCH_SIZE) {
            const size_t count =
                (start + BATCH_SIZE <= TRAIN_COUNT) ? BATCH_SIZE : (TRAIN_COUNT - start);
            FMat xb = slice_rows(&x_train_s, start, count);
            FMat yb = slice_rows(&data.y_train, start, count);
            loss_sum += net_train_step(&net, &xb, &yb) * (float)count;
            fmat_destroy(&xb);
            fmat_destroy(&yb);
        }
        loss_sum /= (float)TRAIN_COUNT;

        net_evaluate(&net, &scaler, &data.x_test, &data.y_test, &eval);
        if (epoch == 1 || epoch % 50 == 0 || epoch == EPOCHS) {
            printf("epoch %4d/%d  train CE %.4f  test accuracy %.1f%%\n", epoch, EPOCHS, loss_sum,
                   eval.accuracy * 100.0f);
        }
    }

    // 4. Honest evaluation on the held-out test set.
    net_evaluate(&net, &scaler, &data.x_test, &data.y_test, &eval);
    printf("\nconfusion matrix (rows = true species, cols = predicted):\n");
    printf("%-12s", "");
    for (size_t c = 0; c < IRIS_CLASSES; c++) printf("%12s", iris_species_names[c]);
    printf("   recall\n");
    for (size_t t = 0; t < IRIS_CLASSES; t++) {
        printf("%-12s", iris_species_names[t]);
        for (size_t p = 0; p < IRIS_CLASSES; p++) printf("%12zu", eval.matrix[t][p]);
        printf("  %5.1f%%\n", eval.recall[t] * 100.0f);
    }
    printf("\ntest accuracy: %.1f%%\n\n", eval.accuracy * 100.0f);

    // 5. Inference on brand-new specimens, like a deployed model.
    printf("inference on unseen specimens:\n");
    const float new_flower_1[IRIS_FEATURES] = {5.0f, 3.5f, 1.5f, 0.3f};
    const float new_flower_2[IRIS_FEATURES] = {6.5f, 3.0f, 5.2f, 2.0f};
    const float new_flower_3[IRIS_FEATURES] = {6.0f, 2.9f, 4.5f, 1.5f};
    predict_specimen(&net, &scaler, new_flower_1); /* clearly setosa */
    predict_specimen(&net, &scaler, new_flower_2); /* clearly virginica */
    predict_specimen(&net, &scaler, new_flower_3); /* borderline versicolor */

    // Cleanup
    net_destroy(&net);
    scaler_destroy(&scaler);
    fmat_destroy(&x_train_s);
    fmat_destroy(&x_test_s);
    split_destroy(&data);
    return 0;
}

