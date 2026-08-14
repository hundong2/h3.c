/*
 * 학습 목표: h3 공개 헤더의 기본값과 캔버스 불변식을 모델 없이 확인한다.
 * 실행: cc -std=c11 -Wall -Wextra -Wpedantic inspect_defaults.c -o inspect_defaults
 */
#include "../../h3.h"

#include <stdio.h>

static int canvas_is_valid(int width, int height) {
    if (width < 32 || height < 32) return 0;
    if (width % 32 != 0 || height % 32 != 0) return 0;

    /* 곱셈 전에 나눗셈으로 검사하면 큰 입력의 정수 overflow를 피할 수 있다. */
    return width <= (768 * 1344) / height;
}

static void print_canvas_check(int width, int height) {
    printf("%dx%d: %s\n", width, height,
           canvas_is_valid(width, height) ? "valid" : "invalid");
}

int main(void) {
    const h3_params defaults = H3_PARAMS_DEFAULT;

    printf("h3 version: %s\n", H3_VERSION);
    printf("defaults: %dx%d, frames=%d, steps=%d, layers=%d\n",
           defaults.width, defaults.height, defaults.frames,
           defaults.steps, defaults.dit_layers);

    print_canvas_check(512, 512);
    print_canvas_check(513, 512);
    print_canvas_check(1344, 768);

    return 0;
}
