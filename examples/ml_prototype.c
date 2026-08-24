/**
 * @file ml_prototype.c
 * @brief End-to-end showcase of solidc's ML math stack.
 *
 * Four self-contained demos, all built exclusively on the primitives in
 * linear_alg.h / matrix.h:
 *
 *   1. Neural network  - a 2 -> 16 -> 3 MLP with ReLU + softmax trained
 *                        via hand-written backprop (SGD) on the classic
 *                        three-spiral dataset.
 *   2. k-means         - unsupervised clustering of the same data.
 *   3. PCA             - dimensionality reduction of correlated 5D data,
 *                        powered by fmat_svd().
 *   4. Least squares   - polynomial regression solved in closed form via
 *                        the Moore-Penrose pseudo-inverse.
 *
 * Build & run from the repo root:
 *   gcc -std=gnu11 -O2 -D_GNU_SOURCE -Iinclude \
 *       examples/ml_prototype.c -o ml_prototype -lm && ./ml_prototype
 */

#include "../include/linear_alg.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define TRAIN_SAMPLES 600
#define EPOCHS        250
#define BATCH_SIZE    32
#define LEARNING_RATE 0.05f
#define HIDDEN        24


/** Copies rows [start, start+count) of m into a fresh matrix. */
static FMat fmat_slice_rows(const FMat* m, size_t start, size_t count) {
    FMat r = fmat_create(count, m->cols);
    if (!r.data || !m->data) {
        fmat_destroy(&r);
        return (FMat){0, 0, NULL};
    }
    for (size_t i = 0; i < count; i++) {
        memcpy(r.data + i * m->cols, m->data + (start + i) * m->cols, m->cols * sizeof(float));
    }
    return r;
}

/* ------------------------------------------------------------------ */
/* Dataset generation                                                  */
/* ------------------------------------------------------------------ */

/**
 * Three-class dataset: inner ring, outer ring, and an off-center blob.
 * Not linearly separable (the rings surround the blob), so the demo
 * actually shows the network learning a nonlinear boundary.
 */
static void make_dataset(FMat* X, FMat* Y_onehot, uint64_t seed) {
    FMatRng rng;
    fmat_rng_seed(&rng, seed);
    fmat_fill(X, 0.0f);
    fmat_fill(Y_onehot, 0.0f);
    for (size_t i = 0; i < TRAIN_SAMPLES; i++) {
        const size_t cls = i % 3;
        const float t = (float)i * 12.9898f;
        if (cls == 2) {
            fmat_set(X, i, 0, 4.5f + fmat_rng_normal(&rng) * 0.6f);
            fmat_set(X, i, 1, 4.5f + fmat_rng_normal(&rng) * 0.6f);
        } else {
            const float rad = (cls == 0) ? 2.0f : 4.2f;
            fmat_set(X, i, 0, rad * sinf(t) + fmat_rng_normal(&rng) * 0.35f);
            fmat_set(X, i, 1, rad * cosf(t) + fmat_rng_normal(&rng) * 0.35f);
        }
        fmat_set(Y_onehot, i, cls, 1.0f);
    }
}

/* ------------------------------------------------------------------ */
/* Demo 1: tiny neural network with hand-rolled backprop               */
/* ------------------------------------------------------------------ */

typedef struct {
    FMat W1, b1;             // h = tanh(X W1 + b1), X: N x 2, W1: 2 x HIDDEN
    FMat W2, b2;             // p = softmax(h W2 + b2), W2: HIDDEN x 3
    FMat vW1, vb1, vW2, vb2; // SGD momentum velocity buffers
} TinyMLP;

static void mlp_init(TinyMLP* net, FMatRng* rng) {
    net->W1 = fmat_create(2, HIDDEN);
    net->b1 = fmat_create(1, HIDDEN);
    net->W2 = fmat_create(HIDDEN, 3);
    net->b2 = fmat_create(1, 3);
    fmat_xavier_init(&net->W1, 2, HIDDEN, rng);
    fmat_xavier_init(&net->W2, HIDDEN, 3, rng);
    net->vW1 = fmat_create(2, HIDDEN);
    net->vb1 = fmat_create(1, HIDDEN);
    net->vW2 = fmat_create(HIDDEN, 3);
    net->vb2 = fmat_create(1, 3);
}

static void mlp_destroy(TinyMLP* net) {
    fmat_destroy(&net->W1);
    fmat_destroy(&net->b1);
    fmat_destroy(&net->W2);
    fmat_destroy(&net->b2);
    fmat_destroy(&net->vW1);
    fmat_destroy(&net->vb1);
    fmat_destroy(&net->vW2);
    fmat_destroy(&net->vb2);
}

/** Forward pass. Stores pre-activations for backprop; returns probabilities. */
static FMat mlp_forward(const TinyMLP* net, const FMat* X, FMat* out_hidden_pre, FMat* out_hidden) {
    FMat z1 = fmat_mul(X, &net->W1);
    fmat_add_row_vector(&z1, &net->b1);
    *out_hidden_pre = fmat_copy(&z1);
    fmat_tanh_ip(&z1);
    *out_hidden = z1;  // move

    FMat z2 = fmat_mul(out_hidden, &net->W2);
    fmat_add_row_vector(&z2, &net->b2);
    FMat probs = fmat_softmax_rows(&z2);
    fmat_destroy(&z2);
    return probs;
}

/** One mini-batch SGD step. Returns cross-entropy loss before the update. */
static float mlp_train_step(TinyMLP* net, const FMat* X, const FMat* Y) {
    const float n = (float)X->rows;

    FMat h_pre, h, probs;
    probs = mlp_forward(net, X, &h_pre, &h);

    const float loss = fmat_cross_entropy(&probs, Y);

    // dL/dZ2 = (softmax - onehot) / N   [softmax + CE shortcut]
    FMat dz2 = fmat_sub(&probs, Y);
    fmat_scale_ip(&dz2, 1.0f / n);

    // Gradients for layer 2
    FMat dw2 = fmat_mul_ta(&h, &dz2);          // h^T dz2 : HIDDEN x 3
    FMat db2 = fmat_sum_cols(&dz2);            // 1 x 3

    // Propagate into hidden layer: tanh'(z) = 1 - tanh(z)^2
    FMat dh = fmat_mul_tb(&dz2, &net->W2);     // dz2 W2^T : N x HIDDEN
    FMat hh = fmat_hadamard(&h, &h);
    FMat one = fmat_create(h.rows, h.cols);
    fmat_fill(&one, 1.0f);
    FMat gate = fmat_sub(&one, &hh);
    FMat da1 = fmat_hadamard(&dh, &gate);

    // Gradients for layer 1
    FMat dw1 = fmat_mul_ta(X, &da1);           // X^T da1 : 2 x HIDDEN
    FMat db1 = fmat_sum_cols(&da1);            // 1 x HIDDEN

    // SGD with momentum: v = 0.9v + g; W -= lr * v
    const float mom = 0.9f;
    FMat step;

    FMat tmp = fmat_scale(&net->vW2, mom);
    fmat_add_ip(&tmp, &dw2);
    fmat_destroy(&net->vW2);
    net->vW2 = tmp;
    step = fmat_scale(&net->vW2, LEARNING_RATE);
    fmat_sub_ip(&net->W2, &step);
    fmat_destroy(&step);

    tmp = fmat_scale(&net->vb2, mom);
    fmat_add_ip(&tmp, &db2);
    fmat_destroy(&net->vb2);
    net->vb2 = tmp;
    step = fmat_scale(&net->vb2, LEARNING_RATE);
    fmat_sub_ip(&net->b2, &step);
    fmat_destroy(&step);

    tmp = fmat_scale(&net->vW1, mom);
    fmat_add_ip(&tmp, &dw1);
    fmat_destroy(&net->vW1);
    net->vW1 = tmp;
    step = fmat_scale(&net->vW1, LEARNING_RATE);
    fmat_sub_ip(&net->W1, &step);
    fmat_destroy(&step);

    tmp = fmat_scale(&net->vb1, mom);
    fmat_add_ip(&tmp, &db1);
    fmat_destroy(&net->vb1);
    net->vb1 = tmp;
    step = fmat_scale(&net->vb1, LEARNING_RATE);
    fmat_sub_ip(&net->b1, &step);
    fmat_destroy(&step);

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
    fmat_destroy(&h_pre);
    fmat_destroy(&h);
    return loss;
}

/** Fraction of samples whose argmax prediction matches the label. */
static float mlp_accuracy(const TinyMLP* net, const FMat* X, const FMat* Y) {
    FMat h_pre, h;
    FMat probs = mlp_forward(net, X, &h_pre, &h);
    FMat pred = fmat_argmax_rows(&probs);
    FMat truth = fmat_argmax_rows(Y);

    size_t correct = 0;
    for (size_t i = 0; i < pred.rows; i++) {
        if (fmat_get(&pred, i, 0) == fmat_get(&truth, i, 0)) correct++;
    }

    const float acc = (float)correct / (float)pred.rows;
    fmat_destroy(&probs);
    fmat_destroy(&pred);
    fmat_destroy(&truth);
    fmat_destroy(&h_pre);
    fmat_destroy(&h);
    return acc;
}

static void demo_neural_net(void) {
    printf("==============================================================\n");
    printf(" 1. NEURAL NETWORK  (2-%d-3 MLP, tanh + softmax, mini-batch SGD)\n", HIDDEN);
    printf("==============================================================\n");

    FMat X = fmat_create(TRAIN_SAMPLES, 2);
    FMat Y = fmat_create(TRAIN_SAMPLES, 3);
    make_dataset(&X, &Y, 20260824u);

    // Feature standardization helps SGD conditioning on raw coordinates.
    FMat mu = fmat_mean_cols(&X);
    for (size_t r = 0; r < X.rows; r++)
        for (size_t c = 0; c < X.cols; c++) fmat_set(&X, r, c, fmat_get(&X, r, c) - fmat_get(&mu, 0, c));
    fmat_destroy(&mu);
    double sq = 0.0;
    for (size_t i = 0; i < X.rows * X.cols; i++) sq += (double)X.data[i] * X.data[i];
    fmat_scale_ip(&X, 1.0f / sqrtf((float)(sq / (X.rows * X.cols))));

    FMatRng rng;
    fmat_rng_seed(&rng, 1337u);
    TinyMLP net;
    mlp_init(&net, &rng);

    printf("  accuracy before training: %5.1f%%\n", mlp_accuracy(&net, &X, &Y) * 100.0f);

    // Shuffled mini-batch SGD: Fisher-Yates over sample indices each epoch.
    size_t* order = malloc(TRAIN_SAMPLES * sizeof(size_t));
    for (size_t i = 0; i < TRAIN_SAMPLES; i++) order[i] = i;

    for (int epoch = 1; epoch <= EPOCHS; epoch++) {
        for (size_t i = TRAIN_SAMPLES - 1; i > 0; i--) {
            const size_t j = (size_t)(fmat_rng_uniform(&rng) * (float)i);
            const size_t tswap = order[i];
            order[i] = order[j];
            order[j] = tswap;
        }

        float loss_sum = 0.0f;
        for (size_t start = 0; start < TRAIN_SAMPLES; start += BATCH_SIZE) {
            const size_t count =
                (start + BATCH_SIZE <= TRAIN_SAMPLES) ? BATCH_SIZE : (TRAIN_SAMPLES - start);
            FMat xb = fmat_slice_rows(&X, start, count);
            FMat yb = fmat_slice_rows(&Y, start, count);
            loss_sum += mlp_train_step(&net, &xb, &yb) * (float)count;
            fmat_destroy(&xb);
            fmat_destroy(&yb);
        }
        loss_sum /= (float)TRAIN_SAMPLES;

        if (epoch % 25 == 0 || epoch == 1) {
            printf("  epoch %4d/%d  loss %.4f  accuracy %5.1f%%\n", epoch, EPOCHS, loss_sum,
                   mlp_accuracy(&net, &X, &Y) * 100.0f);
        }
    }
    free(order);
    printf("  FINAL accuracy: %.1f%%\n\n", mlp_accuracy(&net, &X, &Y) * 100.0f);

    mlp_destroy(&net);
    fmat_destroy(&X);
    fmat_destroy(&Y);
}

/* ------------------------------------------------------------------ */
/* Demo 2: k-means clustering                                          */
/* ------------------------------------------------------------------ */

#define KMEANS_K 3

static void demo_kmeans(void) {
    printf("==============================================================\n");
    printf(" 2. K-MEANS CLUSTERING  (k=%d, %d points)\n", KMEANS_K, TRAIN_SAMPLES);
    printf("==============================================================\n");

    FMat X = fmat_create(TRAIN_SAMPLES, 2);
    FMat Y_dummy = fmat_create(TRAIN_SAMPLES, 3);
    make_dataset(&X, &Y_dummy, 777u);

    FMatRng rng;
    fmat_rng_seed(&rng, 99u);

    // Init centroids to random samples (Forgy method).
    FMat centroids = fmat_create(KMEANS_K, 2);
    for (size_t k = 0; k < KMEANS_K; k++) {
        const size_t idx = (size_t)(fmat_rng_uniform(&rng) * (float)(TRAIN_SAMPLES - 1));
        fmat_set(&centroids, k, 0, fmat_get(&X, idx, 0));
        fmat_set(&centroids, k, 1, fmat_get(&X, idx, 1));
    }

    FMat assign = fmat_create(TRAIN_SAMPLES, 1);
    for (int iter = 0; iter < 100; iter++) {
        // Assignment step: nearest centroid by squared Euclidean distance.
        for (size_t i = 0; i < TRAIN_SAMPLES; i++) {
            float best_d = 1e30f;
            size_t best_k = 0;
            for (size_t k = 0; k < KMEANS_K; k++) {
                const float dx = fmat_get(&X, i, 0) - fmat_get(&centroids, k, 0);
                const float dy = fmat_get(&X, i, 1) - fmat_get(&centroids, k, 1);
                const float d = dx * dx + dy * dy;
                if (d < best_d) {
                    best_d = d;
                    best_k = k;
                }
            }
            fmat_set(&assign, i, 0, (float)best_k);
        }

        // Update step: centroid = mean of assigned points.
        FMat counts = fmat_create(KMEANS_K, 1);
        FMat sums = fmat_create(KMEANS_K, 2);
        for (size_t i = 0; i < TRAIN_SAMPLES; i++) {
            const size_t kk = (size_t)fmat_get(&assign, i, 0);
            fmat_set(&counts, kk, 0, fmat_get(&counts, kk, 0) + 1.0f);
            fmat_set(&sums, kk, 0, fmat_get(&sums, kk, 0) + fmat_get(&X, i, 0));
            fmat_set(&sums, kk, 1, fmat_get(&sums, kk, 1) + fmat_get(&X, i, 1));
        }
        bool moved = false;
        for (size_t k = 0; k < KMEANS_K; k++) {
            if (fmat_get(&counts, k, 0) > 0.0f) {
                const float inv = 1.0f / fmat_get(&counts, k, 0);
                for (size_t c = 0; c < 2; c++) {
                    const float next = fmat_get(&sums, k, c) * inv;
                    if (fabsf(next - fmat_get(&centroids, k, c)) > 1e-5f) moved = true;
                    fmat_set(&centroids, k, c, next);
                }
            }
        }
        fmat_destroy(&counts);
        fmat_destroy(&sums);

        if (!moved) {
            printf("  converged after %d iterations\n", iter + 1);
            break;
        }
    }

    // Report cluster sizes and inertia.
    float inertia = 0.0f;
    size_t sizes[KMEANS_K] = {0};
    for (size_t i = 0; i < TRAIN_SAMPLES; i++) {
        const size_t kk = (size_t)fmat_get(&assign, i, 0);
        sizes[kk]++;
        const float dx = fmat_get(&X, i, 0) - fmat_get(&centroids, kk, 0);
        const float dy = fmat_get(&X, i, 1) - fmat_get(&centroids, kk, 1);
        inertia += dx * dx + dy * dy;
    }
    printf("  cluster sizes:");
    for (size_t k = 0; k < KMEANS_K; k++) printf(" [%zu]=%zu", k, sizes[k]);
    printf("\n  final inertia: %.2f\n", inertia);
    printf("  centroids: (%.2f, %.2f) (%.2f, %.2f) (%.2f, %.2f)\n\n", fmat_get(&centroids, 0, 0),
           fmat_get(&centroids, 0, 1), fmat_get(&centroids, 1, 0), fmat_get(&centroids, 1, 1),
           fmat_get(&centroids, 2, 0), fmat_get(&centroids, 2, 1));

    fmat_destroy(&X);
    fmat_destroy(&Y_dummy);
    fmat_destroy(&centroids);
    fmat_destroy(&assign);
}

/* ------------------------------------------------------------------ */
/* Demo 3: PCA                                                         */
/* ------------------------------------------------------------------ */

#define PCA_SAMPLES 500
#define PCA_DIMS    5

static void demo_pca(void) {
    printf("==============================================================\n");
    printf(" 3. PCA  (%d samples, %d-D -> 2-D)\n", PCA_SAMPLES, PCA_DIMS);
    printf("==============================================================\n");
    printf("  data: 2 latent coordinates drive all %d features,\n", PCA_DIMS);
    printf("  so PCA should find ~2 dominant components.\n\n");

    FMatRng rng;
    fmat_rng_seed(&rng, 4242u);

    FMat X = fmat_create(PCA_SAMPLES, PCA_DIMS);
    for (size_t i = 0; i < PCA_SAMPLES; i++) {
        const float a = fmat_rng_normal(&rng);
        const float b = fmat_rng_normal(&rng);
        // Each feature is a different fixed mix of the two latents + noise.
        static const float mix[PCA_DIMS][2] = {{1.0f, 0.0f}, {0.8f, 0.6f}, {0.2f, 1.0f}, {-0.5f, 0.9f}, {0.7f, -0.7f}};
        for (size_t f = 0; f < PCA_DIMS; f++) {
            fmat_set(&X, i, f, a * mix[f][0] + b * mix[f][1] + fmat_rng_normal(&rng) * 0.05f);
        }
    }

    PCAResult pca;
    if (!fmat_pca(&X, 2, &pca)) {
        printf("  PCA failed!\n");
        fmat_destroy(&X);
        return;
    }

    printf("  explained variance ratio: PC1=%.1f%%  PC2=%.1f%%\n",
           fmat_get(&pca.explained_ratio, 0, 0) * 100.0f, fmat_get(&pca.explained_ratio, 1, 0) * 100.0f);

    FMat proj = fmat_pca_transform(&pca, &X);
    printf("  projected shape: %zu x %zu\n", proj.rows, proj.cols);

    // Show that the first projected coordinate carries most of the spread.
    double v0 = 0.0, v1 = 0.0;
    for (size_t i = 0; i < proj.rows; i++) {
        v0 += (double)fmat_get(&proj, i, 0) * fmat_get(&proj, i, 0);
        v1 += (double)fmat_get(&proj, i, 1) * fmat_get(&proj, i, 1);
    }
    printf("  projected variance ratio: %.1fx more spread on PC1 than PC2\n\n", v0 / v1);

    fmat_destroy(&X);
    fmat_destroy(&proj);
    pca_result_destroy(&pca);
}

/* ------------------------------------------------------------------ */
/* Demo 4: least squares regression                                    */
/* ------------------------------------------------------------------ */

static void demo_lstsq(void) {
    printf("==============================================================\n");
    printf(" 4. LEAST SQUARES  (polynomial regression via pinv)\n");
    printf("==============================================================\n");

    // Ground truth: y = 0.5x^2 - 1.5x + 2 plus noise.
    const float true_a = 0.5f, true_b = -1.5f, true_c = 2.0f;
    const size_t n = 40;

    FMatRng rng;
    fmat_rng_seed(&rng, 31415u);

    FMat A = fmat_create(n, 3); // features [x^2, x, 1]
    FMat y = fmat_create(n, 1);
    for (size_t i = 0; i < n; i++) {
        const float x = -3.0f + 6.0f * (float)i / (float)(n - 1);
        const float noise = fmat_rng_normal(&rng) * 0.15f;
        fmat_set(&A, i, 0, x * x);
        fmat_set(&A, i, 1, x);
        fmat_set(&A, i, 2, 1.0f);
        fmat_set(&y, i, 0, true_a * x * x + true_b * x + true_c + noise);
    }

    FMat coef;
    fmat_lstsq(&A, &y, &coef);

    printf("  ground truth: y = %.2fx^2 + %.2fx + %.2f\n", true_a, true_b, true_c);
    printf("  fitted:       y = %.3fx^2 %+.3fx %+.3f\n\n", fmat_get(&coef, 0, 0), fmat_get(&coef, 1, 0),
           fmat_get(&coef, 2, 0));

    fmat_destroy(&A);
    fmat_destroy(&y);
    fmat_destroy(&coef);
}

int main(void) {
    printf("\nsolidc ML prototype -- built on linear_alg.h primitives only\n\n");
    demo_neural_net();
    demo_kmeans();
    demo_pca();
    demo_lstsq();
    printf("done.\n");
    return 0;
}
