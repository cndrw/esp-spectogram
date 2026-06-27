#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include <inttypes.h>

typedef struct {
    void(*execute)(int32_t* buffer, size_t size);
} pipeline_t;

void raw_audio_pipeline(int32_t* buffer, size_t size);

void init_dsps_fft2r();
void fft_pipeline(int32_t* buffer, size_t size);

void spectogram_pipeline(int32_t* buffer, size_t size);

#endif