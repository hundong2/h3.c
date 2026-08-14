/*
 * 학습 목표: 공개 h3 API의 load -> generate -> free 수명주기를 익힌다.
 * 이 예제는 Apple Silicon, 빌드된 libh3.a, 모델, FFmpeg가 필요하다.
 */
#include "../../h3.h"

#include <stdio.h>

static int report_progress(const char *phase, int completed, int total,
                           void *opaque) {
    (void)opaque;
    fprintf(stderr, "[%s] %d/%d\n", phase, completed, total);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s MODEL_DIR OUTPUT_MP4\n", argv[0]);
        return 2;
    }

    h3_ctx *ctx = h3_load_dir(argv[1]);
    if (!ctx) {
        /* 초기화 실패는 전역 오류 문자열을 NULL context로 조회할 수 있다. */
        fprintf(stderr, "model load failed: %s\n", h3_last_error(NULL));
        return 1;
    }

    h3_params params = H3_PARAMS_DEFAULT;
    params.width = 256;
    params.height = 256;
    params.frames = 22;
    params.steps = 4;
    params.dit_layers = 50;
    params.denoise_reuse = 1;
    params.output_path = argv[2];
    params.on_progress = report_progress;

    const char *prompt =
        "A paper boat floating on a calm pond, static camera, soft morning "
        "light, gentle water sounds, no music.";
    h3_result *result = h3_generate(ctx, prompt, &params);
    if (!result) {
        fprintf(stderr, "generation failed: %s\n", h3_last_error(ctx));
        h3_free(ctx);
        return 1;
    }

    printf("created %dx%d, %d frames at %d fps, seed=%llu\n",
           result->width, result->height, result->frames, result->fps,
           (unsigned long long)result->seed);

    h3_result_free(result);
    h3_free(ctx);
    return 0;
}
