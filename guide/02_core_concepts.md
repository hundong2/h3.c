# 02. 핵심 개념과 C API

## 1. 전체 실행 흐름

```text
프롬프트/참조 미디어
  → tokenizer + Qwen 텍스트/비전 인코더
  → FL2VA 또는 Ref2VA 조건 레이아웃
  → DiT가 비디오·오디오 latent를 공동 디노이징
  → Video VAE + AudioVAE/BigVGAN 디코딩
  → 프레임 콜백 + FFmpeg H.264/AAC mux
```

- **FL2VA**: 텍스트와 선택적인 첫·마지막 프레임으로 비디오/오디오를 생성한다.
- **Ref2VA**: 순서가 있는 이미지·비디오·오디오 참조를 조건으로 사용한다.
- **DiT(Diffusion Transformer)**: 잡음 latent를 여러 스텝에 걸쳐 생성 결과로 바꾼다.
- **VAE**: 픽셀/파형과 작은 latent 표현 사이를 변환한다.
- **BF16/int8**: 메모리와 연산량을 줄이는 수치 표현이다. 빠른 경로는 기준 경로와 완전히 같은 이미지가 아닐 수 있다.

## 2. 공개 C API 수명주기

`h3.h`의 최소 흐름은 다음과 같다.

1. `h3_load_dir(model_dir)`로 메타데이터와 Metal 장치를 초기화한다.
2. 실패하면 `h3_last_error(ctx)`를 확인한다.
3. `H3_PARAMS_DEFAULT`를 복사하고 필요한 필드만 변경한다.
4. `h3_generate(ctx, prompt, &params)`를 호출한다.
5. `h3_result_free(result)`, `h3_free(ctx)` 순서로 해제한다.

완전한 코드는 [generate_video.c](examples/generate_video.c)에 있다.

```c
h3_ctx *ctx = h3_load_dir(model_dir);
h3_params params = H3_PARAMS_DEFAULT;
params.width = 512;
params.height = 512;
params.frames = 22;
params.steps = 20;
params.dit_layers = 45;
params.denoise_reuse = 2;
params.output_path = "outputs/api.mp4";

h3_result *result = h3_generate(ctx, prompt, &params);
```

`H3_PARAMS_DEFAULT`는 지정 초기화가 아니라 필드 순서 기반 매크로이므로 공개 구조체 필드를 추가할 때 매크로도 반드시 갱신해야 한다.

## 3. 콜백

`on_progress`는 단계 이름과 진행량을, `on_frame`은 RGB24 프레임을 전달한다. 프레임 데이터는 콜백 호출 동안만 읽고, 이후 보관하려면 `stride * height` 바이트를 복사한다. 콜백이 0이 아닌 값을 반환하면 호출자가 중단을 요청한 것으로 처리될 수 있으므로 정상 진행 시 0을 반환한다.

## 4. 참조 입력

`h3_reference` 배열은 순서 자체가 모델 입력 의미를 가진다. 문자열 경로와 배열은 `h3_generate`가 사용하는 동안 유효해야 한다.

```c
h3_reference refs[] = {
    { H3_REFERENCE_IMAGE, "person.png", NULL, 0 },
    { H3_REFERENCE_AUDIO, "music.wav", NULL, 0 },
};
params.references = refs;
params.reference_count = 2;
```

독립 오디오는 이미지/비디오와 함께 사용하고 2~15초, 최대 3개, 총 15초 제한을 지킨다. FL2VA 첫·마지막 프레임과 Ref2VA 배열은 섞지 않는다.

## 5. 형상 불변식

- 가로·세로: 각각 32의 배수, 최소 32
- 총 픽셀: `768 * 1344` 이하
- 내부 렌더 크기: 둘 다 0이거나 함께 지정, 출력 이하, 같은 종횡비
- 프레임: 내부적으로 `5 + 17*n`에 올림 정렬
- `denoise_reuse`와 `core_reuse`: 동시에 빠른 값으로 설정하지 않음

`h3_host.c`의 순수 호스트 함수와 `tests/test_h3.c`는 이 규칙을 이해하기 좋은 출발점이다.

## 6. 캐시와 대화형 실행

one-shot 호출은 단계별 메모리 해제를 우선한다. 반복 요청에서는 `h3_cache_set_enabled(ctx, 1)`로 조건 임베딩, 준비된 DiT, 디코더를 재사용할 수 있다. 프롬프트·참조 파일 메타데이터·형상·성능 옵션이 cache key에 포함된다. 설정 변경 뒤 메모리를 즉시 회수하려면 `h3_cache_clear`를 호출하고 `h3_cache_get_info`로 상태를 관찰한다.

다음은 [고급 성능·디버깅](03_advanced.md)에서 기준 실험을 설계한다.
