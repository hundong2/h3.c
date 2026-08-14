# 03. 고급 성능·디버깅

## 1. 성능 실험의 기준

성능 최적화와 생성 품질을 동시에 바꾸면 원인을 알 수 없다. 다음을 고정한다.

- 모델 스냅샷과 커밋
- 프롬프트와 참조 미디어
- `--seed`, 출력/내부 크기, 프레임, 스텝
- 전원 모드, macOS 버전, 장치, 가능한 한 비슷한 열 상태

기준은 `--steps 50 --layers 50 --reuse 1` 또는 비교 목적에 맞는 느린 옵션으로 정한다. 빠른 후보와 기준을 ABBA 순서로 번갈아 실행하고 첫 로드 비용과 워밍 실행을 분리한다.

## 2. 프로파일 읽기

`--profile`의 wall time은 사용자가 체감하는 단계 시간이다. Metal root timestamp는 MPSGraph가 내부에서 예약한 child buffer를 빠뜨릴 수 있으므로 wall/commit-to-fence와 함께 본다. peak live tensor는 모델 파일 크기나 전체 시스템 RAM과 같지 않다. cumulative allocation은 동시에 상주한 메모리가 아니라 실행 중 누적 할당량이다.

## 3. 최적화 레버

| 레버 | 얻는 것 | 잃을 수 있는 것 |
|---|---|---|
| 스텝 감소 | 거의 선형적인 DiT 시간 감소 | 디테일·움직임 |
| `--reuse` | 새 DiT 평가 감소 | 속도 외삽 오차 |
| `--layers` | 계산·가중치 감소 | 모델 표현력 |
| `--core-reuse` | 비싼 transformer core 재사용 | 피사체 충실도 |
| token reduction | 중간 블록 시퀀스 감소 | 구도·세부 |
| 내부 캔버스 | DiT/VAE 공간 비용 감소 | 해상도·프레이밍 |
| SSD streaming | DiT 상주 메모리 급감 | I/O 대기·속도 |
| int8 경로 | M5 속도·메모리 개선 | 작은 수치·구도 차이 |

한 번에 하나씩 켜고, 결과 프레임뿐 아니라 전체 비디오의 움직임과 오디오도 비교한다.

## 4. 디버깅 계층

1. `--info`: 모델 구조와 장치만 검사한다.
2. `make test`: 결정론적 호스트 테스트와 사용 가능한 fixture를 검사한다.
3. `make parity`: Metal과 MLX toy fixture 동등성을 검사한다.
4. `--frames-dir ... -o ''`: FFmpeg mux를 분리한다.
5. 느린 `--use-slower-*` 옵션: 특정 융합/양자화 경로를 기준으로 되돌린다.
6. `H3_DISABLE_*`: 커널 융합이나 캐시를 하나씩 끈다.

모델 전체 실패 전에 `tests/test_h3.c`와 같은 작은 호스트 불변식, toy fixture, 실제 한 블록, 전체 생성 순으로 범위를 넓힌다.

## 5. 자주 발생하는 문제

### 모델 경로 오류

`-d`가 shard를 포함한 스냅샷 루트인지, LFS pointer가 아닌 실제 파일인지 확인한다. FL2VA와 Ref2VA는 서로 다른 transformer tree를 사용한다.

### FFmpeg를 찾지 못함

`H3_FFMPEG`, `H3_FFPROBE`를 절대 경로로 지정하거나 `--frames-dir`와 빈 출력 `-o ''`로 인코딩 단계를 제외한다.

### 메모리 부족 또는 swap

`--show`를 끄고 프레임/내부 캔버스를 줄인다. 그래도 부족하면 `--ssd-streaming`을 사용한다. 이때 int8 row FC2와 조합하지 않는다.

### 결과가 깨지거나 비교가 불안정함

시드를 고정하고 token reduction, layer/reuse 공격적 조합을 해제한다. 256에서는 token reduction을 끄고, MLX 좌표 비교라면 `--use-reference-rope`를 쓴다.

### 프레임 수가 요청과 다름

H3가 시간축 형상을 `5 + 17*n`으로 올림한다. 정확한 실제 길이는 `aligned_frames / 24`로 계산한다.

## 6. 확장 지점

- 새 CLI 옵션: `main.c` 파싱·검증, `h3_params`, 대화형 명령, README를 함께 갱신한다.
- 새 조건 모달리티: `h3_reference`, layout segment, encoder, cache key, DiT packing을 함께 설계한다.
- 새 GPU 빠른 경로: 지원 장치 guard와 정확한 fallback을 유지하고 동일 입력 A/B를 제공한다.
- 새 출력 방식: `h3_frame_callback`에서 처리하되 콜백 데이터 수명과 stride를 지킨다.

## 7. 테스트 추가 원칙

경계값(32픽셀, 최대 픽셀, 프레임 정렬), overflow, 누락 파일, 상호 배타 옵션을 먼저 테스트한다. GPU 경로는 단일 커널 microbenchmark만으로 채택하지 말고 완전 DiT 순전파와 실제 생성에서도 교차 측정한다. 품질 차이가 허용되는 최적화는 byte-identical이라고 주장하지 말고 SSIM, 상대 L2와 시각·시간 일관성 검사를 함께 기록한다.
