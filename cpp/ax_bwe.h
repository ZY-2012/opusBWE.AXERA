/* AX650 NPU backend for the Opus OSCE-BWE feature net.
 *
 * Replaces the CPU implementation of bbwe_feature_net() (conv1 / conv2 / GRU /
 * transposed conv) with the quantized axmodel. One frame (10 ms, 114 features)
 * per call; the conv histories and the GRU state are carried in the caller's
 * state buffers (same layout as BBWENetState).
 */
#ifndef AX_BWE_H
#define AX_BWE_H

#ifdef __cplusplus
extern "C" {
#endif

#define AX_BWE_FEATURE_DIM 114
#define AX_BWE_COND_DIM 128
#define AX_BWE_CONV1_STATE 228 /* 114 * 2 */
#define AX_BWE_CONV2_STATE 256 /* 128 * 2 */
#define AX_BWE_GRU_STATE 128

/* Loads the axmodel and initializes the AX runtime. Returns 0 on success. */
int ax_bwe_init(const char *model_path);

/* One 10 ms frame: features[114] -> cond[256] (2 x 128), states updated in place. */
int ax_bwe_cond(float *cond, const float *features,
                float *conv1_state, float *conv2_state, float *gru_state);

void ax_bwe_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* AX_BWE_H */
