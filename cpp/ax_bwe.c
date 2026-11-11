/* AX650 NPU backend for the Opus OSCE-BWE feature net (see ax_bwe.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ax_engine_api.h"
#include "ax_engine_type.h"
#include "ax_sys_api.h"

#include "ax_bwe.h"

static AX_ENGINE_HANDLE_EXTRA_T g_extra;
static void *g_handle = NULL;
static void *g_context = NULL;
static AX_ENGINE_IO_INFO_T *g_info = NULL;
static AX_ENGINE_IO_T *g_io = NULL;
static AX_ENGINE_IO_BUFFER_T *g_in[4];
static AX_ENGINE_IO_BUFFER_T *g_out[4];
static int g_ok = 0;

static int find_io(const char *name, int is_input)
{
    AX_U32 i;
    if (is_input) {
        for (i = 0; i < g_info->nInputSize; ++i)
            if (strcmp(g_info->pInputs[i].pName, name) == 0) return (int)i;
    } else {
        for (i = 0; i < g_info->nOutputSize; ++i)
            if (strcmp(g_info->pOutputs[i].pName, name) == 0) return (int)i;
    }
    fprintf(stderr, "[ax_bwe] tensor %s not found\n", name);
    return -1;
}

int ax_bwe_init(const char *model_path)
{
    FILE *f;
    long size;
    char *data;
    int idx[4];
    AX_U32 i;

    if (AX_SYS_Init() != 0) {
        fprintf(stderr, "[ax_bwe] AX_SYS_Init failed\n");
        return -1;
    }
    {
        AX_ENGINE_NPU_ATTR_T attr;
        memset(&attr, 0, sizeof(attr));
        attr.eHardMode = AX_ENGINE_VIRTUAL_NPU_DISABLE;
        if (AX_ENGINE_Init(&attr) != 0) {
            fprintf(stderr, "[ax_bwe] AX_ENGINE_Init failed\n");
            return -1;
        }
    }

    f = fopen(model_path, "rb");
    if (!f) {
        fprintf(stderr, "[ax_bwe] cannot open %s\n", model_path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (char *)malloc((size_t)size);
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "[ax_bwe] read failed\n");
        fclose(f);
        free(data);
        return -1;
    }
    fclose(f);

    memset(&g_extra, 0, sizeof(g_extra));
    g_extra.pName = (AX_S8 *)"opusbwe";
    if (AX_ENGINE_CreateHandleV2(&g_handle, data, (AX_U32)size, &g_extra) != 0 || !g_handle) {
        fprintf(stderr, "[ax_bwe] CreateHandle failed\n");
        free(data);
        return -1;
    }
    if (AX_ENGINE_CreateContextV2(g_handle, &g_context) != 0 || !g_context) {
        fprintf(stderr, "[ax_bwe] CreateContext failed\n");
        return -1;
    }
    if (AX_ENGINE_GetIOInfo(g_handle, &g_info) != 0 || !g_info) {
        fprintf(stderr, "[ax_bwe] GetIOInfo failed\n");
        return -1;
    }
    g_io = (AX_ENGINE_IO_T *)calloc(1, sizeof(*g_io));
    g_io->nInputSize = g_info->nInputSize;
    g_io->nOutputSize = g_info->nOutputSize;
    g_io->pInputs = (AX_ENGINE_IO_BUFFER_T *)calloc(g_info->nInputSize, sizeof(AX_ENGINE_IO_BUFFER_T));
    g_io->pOutputs = (AX_ENGINE_IO_BUFFER_T *)calloc(g_info->nOutputSize, sizeof(AX_ENGINE_IO_BUFFER_T));

    for (i = 0; i < g_info->nInputSize; ++i) {
        g_io->pInputs[i].nSize = g_info->pInputs[i].nSize;
        if (AX_SYS_MemAllocCached(&g_io->pInputs[i].phyAddr, &g_io->pInputs[i].pVirAddr,
                                  g_io->pInputs[i].nSize, 128, (const AX_S8 *)"opusbwe") != 0)
            return -1;
        memset(g_io->pInputs[i].pVirAddr, 0, g_io->pInputs[i].nSize);
    }
    for (i = 0; i < g_info->nOutputSize; ++i) {
        g_io->pOutputs[i].nSize = g_info->pOutputs[i].nSize;
        if (AX_SYS_MemAllocCached(&g_io->pOutputs[i].phyAddr, &g_io->pOutputs[i].pVirAddr,
                                  g_io->pOutputs[i].nSize, 128, (const AX_S8 *)"opusbwe") != 0)
            return -1;
    }

    g_in[0] = &g_io->pInputs[find_io("features", 1)];
    g_in[1] = &g_io->pInputs[find_io("conv1_state", 1)];
    g_in[2] = &g_io->pInputs[find_io("conv2_state", 1)];
    g_in[3] = &g_io->pInputs[find_io("gru_state", 1)];
    {
        int a = find_io("cond", 0);
        int b = find_io("conv1_state_out", 0);
        int c = find_io("conv2_state_out", 0);
        int d = find_io("gru_state_out", 0);
        if (a < 0 || b < 0 || c < 0 || d < 0) return -1;
        g_out[0] = &g_io->pOutputs[a];
        g_out[1] = &g_io->pOutputs[b];
        g_out[2] = &g_io->pOutputs[c];
        g_out[3] = &g_io->pOutputs[d];
    }
    (void)idx;
    free(data);
    g_ok = 1;
    return 0;
}

int ax_bwe_cond(float *cond, const float *features,
                float *conv1_state, float *conv2_state, float *gru_state)
{
    AX_U32 i;
    if (!g_ok) return -1;

    memcpy(g_in[0]->pVirAddr, features, AX_BWE_FEATURE_DIM * sizeof(float));
    memcpy(g_in[1]->pVirAddr, conv1_state, AX_BWE_CONV1_STATE * sizeof(float));
    memcpy(g_in[2]->pVirAddr, conv2_state, AX_BWE_CONV2_STATE * sizeof(float));
    memcpy(g_in[3]->pVirAddr, gru_state, AX_BWE_GRU_STATE * sizeof(float));
    for (i = 0; i < g_io->nInputSize; ++i)
        AX_SYS_MflushCache(g_io->pInputs[i].phyAddr, g_io->pInputs[i].pVirAddr, g_io->pInputs[i].nSize);

    if (AX_ENGINE_RunSyncV2(g_handle, g_context, g_io) != 0) return -1;

    for (i = 0; i < g_io->nOutputSize; ++i)
        AX_SYS_MinvalidateCache(g_io->pOutputs[i].phyAddr, g_io->pOutputs[i].pVirAddr,
                                g_io->pOutputs[i].nSize);

    memcpy(cond, g_out[0]->pVirAddr, 2 * AX_BWE_COND_DIM * sizeof(float));
    memcpy(conv1_state, g_out[1]->pVirAddr, AX_BWE_CONV1_STATE * sizeof(float));
    memcpy(conv2_state, g_out[2]->pVirAddr, AX_BWE_CONV2_STATE * sizeof(float));
    memcpy(gru_state, g_out[3]->pVirAddr, AX_BWE_GRU_STATE * sizeof(float));
    return 0;
}

void ax_bwe_deinit(void)
{
    AX_U32 i;
    if (g_io) {
        for (i = 0; i < g_io->nInputSize; ++i)
            if (g_io->pInputs[i].pVirAddr)
                AX_SYS_MemFree(g_io->pInputs[i].phyAddr, g_io->pInputs[i].pVirAddr);
        for (i = 0; i < g_io->nOutputSize; ++i)
            if (g_io->pOutputs[i].pVirAddr)
                AX_SYS_MemFree(g_io->pOutputs[i].phyAddr, g_io->pOutputs[i].pVirAddr);
        free(g_io->pInputs);
        free(g_io->pOutputs);
        free(g_io);
        g_io = NULL;
    }
    if (g_handle) {
        AX_ENGINE_DestroyHandle(g_handle);
        g_handle = NULL;
    }
    g_ok = 0;
}
