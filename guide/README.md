# h3-metal 한국어 학습 가이드

이 가이드는 C와 생성 모델을 처음 접한 사용자부터 Metal 커널과 성능 경로를 분석하려는 개발자까지 순서대로 학습하도록 구성했다.

## 학습 순서

1. [설치와 첫 실행](01_getting_started.md): 지원 환경, 모델·FFmpeg 준비, 빌드, 모델 검사, 첫 생성
2. [핵심 개념과 C API](02_core_concepts.md): FL2VA/Ref2VA, DiT·VAE, 공개 API와 실행 흐름
3. [고급 성능·디버깅](03_advanced.md): 프리셋 설계, 메모리 경로, 프로파일링, 테스트 전략
4. [실습 예제](examples/README.md): 모델 없이 설정을 검사하는 C 예제와 실제 생성 API 예제

## 프로젝트가 해결하는 문제

`h3-metal`은 MiniMax-H3의 텍스트·이미지·비디오·오디오 조건을 Apple Silicon의 Metal에서 네이티브 C/Objective-C로 실행한다. Python 런타임에 생성 파이프라인을 맡기지 않고 모델 검사부터 Qwen 조건 인코딩, DiT 디노이징, VAE/BigVGAN 디코딩, FFmpeg mux까지 한 실행 파일로 연결한다.

## 빠른 선택표

| 목표 | 권장 시작점 | 주의점 |
|---|---|---|
| 설치 확인 | `./h3 --info -d ./MiniMax-H3` | 전체 가중치를 매핑하지 않음 |
| 빠른 개발 | 512x512, 22프레임, 20스텝, 45레이어, reuse 2 | 품질 비교는 동일 시드 사용 |
| 저메모리 | `--ssd-streaming`, `--show` 끔 | SSD 읽기로 느려짐 |
| 기준 품질 | 50스텝, 50레이어, reuse 1 | 시간·메모리 비용 큼 |
| 짧은 프리뷰 | 256x256 또는 4~7스텝 | 최종 품질 대체물이 아님 |

## 저장소 지도

| 경로 | 역할 |
|---|---|
| `main.c` | CLI 파싱, 옵션 검증, one-shot/대화형 진입 |
| `h3_cli.c` | Iris 스타일 대화형 세션과 명령 상태 |
| `h3.c`, `h3.h` | 공개 API, 컨텍스트·캐시·생성 수명주기 |
| `h3_host.*` | 캔버스·시간축·레이아웃·스케줄·난수 등 CPU 로직 |
| `h3_text_encoder.*`, `h3_tokenizer.*` | Qwen 텍스트 조건 인코딩 |
| `h3_vision_encoder.*`, `h3_video_encoder.*` | 이미지·비디오 참조 조건 |
| `h3_dit.*`, `h3_dit_schedule.*` | 결합 latent 디노이징과 스케줄 |
| `h3_video_vae.*`, `h3_audio_vae.*` | 비디오·오디오 latent 변환 |
| `h3_metal.*`, `h3_gpu.*`, `h3_shaders.metal` | Metal/MPSGraph/TensorOps 실행 |
| `h3_ffmpeg.*`, `h3_terminal.*` | 미디어 입출력·mux·터미널 프리뷰 |
| `tests/` | 호스트 단위, Metal 동등성, 실제 가중치 fixture 테스트 |

## 권장 학습 방법

먼저 모델 없이 `guide/examples/inspect_defaults.c`를 실행해 공개 상수와 옵션 불변식을 이해한다. 그다음 `--info`, 짧은 256 또는 512 프리뷰로 환경을 검증한다. 품질 비교는 한 변수만 바꾸고 동일한 프롬프트·시드·형상을 유지한다. 마지막으로 `--profile`과 느린 기준 옵션을 사용해 최적화가 속도와 결과에 미치는 영향을 분리한다.

## 원문과 번역

- [원본 README](../README.md)
- [한국어 README](../README_kor.md)

## 기여 전 체크

- 공개 헤더의 ABI와 `H3_PARAMS_DEFAULT` 필드 순서를 함께 확인한다.
- 새 빠른 경로에는 결과 비교가 가능한 느린 오라클 또는 비활성화 스위치를 둔다.
- 호스트 로직은 결정론적 단위 테스트, GPU 로직은 fixture 동등성 테스트를 추가한다.
- 성능 수치는 장치, OS, 입력 형상, 열 상태, 반복 순서를 함께 기록한다.
- `make test`, 필요 시 `make parity`, `git diff --check`를 실행한다.
