# h3-metal

[English](README.md) · [한국어 학습 가이드](guide/README.md)

Apple Silicon에서 MiniMax-H3를 네이티브로 추론하는 프로젝트다. 이 프로젝트는 동작하는 수직 기능 단위를 순차적으로 확장한다. 먼저 결정론적인 호스트/모델 메타데이터를 구현하고, 이어서 이식 가능한 Metal 블록 동등성, 프롬프트 인코딩, 프롬프트 기반 비디오/오디오 생성, 첫·마지막 프레임 조건화, 순서가 있는 참조 입력을 지원한다.

현재 프롬프트 기반 비디오/오디오 생성, 첫·마지막 프레임 조건화, Ref2VA 이미지·비디오·오디오 참조가 종단 간 동작한다. 지금은 M3 Max와 M5 Max에서 H3 전용 Metal 성능과 메모리를 점진적으로 최적화하고 있다.

> 이 문서는 원본 `README.md`의 한국어 번역이다. 명령과 측정값은 원문을 보존했으며, 실제 성능은 하드웨어·macOS·열 상태에 따라 달라질 수 있다.

## 튜토리얼

### 1. 빌드하고 모델 검사하기

예제는 Hugging Face 스냅샷이 `./MiniMax-H3`에 있고 FFmpeg와 FFprobe가 `PATH`에 있다고 가정한다.

```sh
make -j8
mkdir -p outputs
./h3 --info -d ./MiniMax-H3
```

`--info`는 전체 가중치를 매핑하거나 미디어를 생성하지 않고 모델 배치와 선택된 Metal 장치를 출력한다. 전체 CLI 옵션은 `./h3 --help`로 확인한다.

`-p`를 생략하면 같은 바이너리가 Iris 스타일 대화형 세션을 시작한다.

```sh
./h3 -d ./MiniMax-H3 --width 512 --height 512 --steps 6
```

프롬프트를 입력하면 번호가 붙은 비디오를 만든다. 세션은 정확한 BF16 프롬프트 조건, 준비된 DiT, 비디오 디코더를 메모리에 유지하므로 다른 시드로 반복할 때 다시 로드하고 인코딩하는 비용을 줄인다. 주요 명령은 `!status`, `!seed random`, `!seconds 2`, `!show`, `!save output.mp4`, `!cache`이며 전체 목록은 `!help`에서 본다.

첫·마지막 프레임 조건은 세션 동안 유지된다.

```text
h3> !first opening.png
h3> !last ending.png
h3> The camera moves slowly around the subject.
```

앵커는 `!first clear`, `!last clear`로 지운다. 생성 비디오는 시작할 때 표시된 세션 디렉터리에 저장된다. 일반 Ref2VA 조건 이미지는 `!ref-image PATH`를 사용한다. 이미지는 추가 순서대로 `<Picture 1>`, `<Picture 2>`가 되며 파일명 자체에는 의미가 없다.

```text
h3> !ref-image person.png
h3> Make the person shown in Picture 1 wave to the camera.
```

`!refs`는 현재 순서를, `!ref-remove N`은 특정 항목 제거를, `!refs clear`는 전체 제거를 수행한다. Ref2VA 참조와 `!first`/`!last` 앵커는 함께 사용할 수 없다.

### 2. 빠른 첫 비디오 만들기

검증된 균형 프리셋부터 시작한다. 24 fps의 22프레임(약 0.92초)을 만들며, 지원되는 그래픽 터미널에서는 매 디노이징 전이 후 중간 프레임을 표시하고 단계별 시간을 출력한다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A red fox walks through fresh snow in a pine forest. Medium tracking shot, natural winter light, realistic fur, soft footsteps and wind." \
  --width 512 --height 512 \
  --frames 22 --steps 20 \
  --layers 45 --reuse 2 \
  --show \
  -o outputs/fox-fast.mp4
```

- `--steps 20`: 기본 20회 디노이징 패스를 수행한다.
- `--reuse 2`: 20번 모두 계산하는 대신 11번의 새 디노이저 속도를 계산하고 건너뛴 전이를 외삽한다.
- `--layers 45`: 50개 트랜스포머 블록 중 45개를 실행해 시간과 통합 메모리를 줄인다.
- `--show`: 선택 사항이다. Kitty/Ghostty 및 iTerm2/WezTerm/Konsole 프로토콜을 지원한다. Retina 기본 배율은 2이며 비 HiDPI에서는 `--zoom 1`을 쓴다. 상주 프리뷰 VAE로 약 10 GiB가 추가될 수 있다.
- `--profile`: 선택 사항이며 생성 경로 자체를 바꾸지 않는다.

첫 프로세스는 모델 로드와 파일시스템 캐시 비용도 부담한다. 반복 실행을 비교하고, 열 스로틀링 영향을 줄이기 위해 장비가 데워지는 동안 변형을 번갈아 측정한다.

매우 짧은 반복 실험은 4패스를 직접 지정한다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A red fox walks through fresh snow in a pine forest. Medium tracking shot, natural winter light, realistic fur." \
  --width 512 --height 512 --frames 22 \
  --steps 4 --layers 50 --reuse 1 \
  --show \
  -o outputs/fox-four-step.mp4
```

`--steps N`은 항상 정확히 N회의 디노이징 패스를 뜻한다. 4~7패스는 저예산 비교에서 선택된 스케줄을 쓰며 패스를 늘리면 디테일과 움직임이 개선된다. 작은 패스 수에서는 요청한 모든 패스를 계산하도록 `--reuse 1`을 유지한다. 512 정사각형 22프레임 여우 테스트의 4패스 결과는 29패스 기준 대비 전체 비디오 SSIM 0.556, 독립 서퍼 테스트는 0.547이었다. M5 Max에서 4패스 디노이징은 약 3.5초, 기준 경로는 26.4초였다.

저메모리 실행에는 `--ssd-streaming`을 추가한다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A red fox walks through fresh snow in a pine forest." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 50 --reuse 1 --ssd-streaming \
  -o outputs/fox-ssd.mp4
```

변환·양자화하지 않은 원본 BF16 체크포인트를 사용하면서 DiT 블록 두 개만 메모리에 두고 GPU가 현재 블록을 처리하는 동안 SSD에서 다음 블록을 읽는다. M5 Max에서 추적된 DiT 저장 공간은 512 정사각형에서 약 36.5 GiB→2.0 GiB, 864x480에서 2.1 GiB로 줄었다. 워밍된 50블록 순전파는 각각 1.35→2.49초(84% 느림), 2.14→2.68초(26% 느림)였고 두 검사 모두 바이트 단위 결과가 같았다. 이 수치는 전체 RAM이 아니라 DiT 텐서 저장 공간이다. `--show`는 약 10 GiB를 더 쓰므로 최저 메모리가 목적이면 끈다. `--ssd-streaming`과 `--use-int8-row-fc2`는 함께 사용할 수 없다.

### 3. 기준 품질에 접근하기

품질을 평가할 때는 한 번에 하나의 제어만 바꾼다. 전체 레이어, 전체 디노이저 평가, 느린 50패스 기준 순서로 복원한다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A red fox walks through fresh snow in a pine forest. Medium tracking shot, natural winter light, realistic fur, soft footsteps and wind." \
  --width 512 --height 512 \
  --frames 22 --steps 50 \
  --layers 50 --reuse 1 \
  -o outputs/fox-close.mp4
```

기본값은 `--steps 20 --layers 50 --reuse 1`이다. 기준 경로는 50개 블록 순전파를 50번 수행하므로 훨씬 비싸지만 빠른 모드가 피사체·해부학·움직임·구도를 바꾸는지 판별하는 오라클로 적합하다. 난수와 실행 엔진이 다르므로 MLX와 픽셀 단위 동일성은 기대하지 않되, 묘사 내용과 움직임은 일치해야 한다.

### 4. 속도/품질 프리셋 선택하기

| 제어 | 느린 기준 | 기본 | 공격적 | 주요 영향 |
|---|---:|---:|---:|---|
| 디노이징 패스 | `--steps 50` | `--steps 20` | `--steps 4..7` | 실제 디노이징 횟수 |
| 전체 디노이저 재사용 | `--reuse 1` | `--reuse 2` | `--reuse 3` | 20스텝에서 새 DiT 평가 20/11/8회 |
| 활성 DiT 블록 | `--layers 50` | `--layers 45` | `--layers 40` | 계산량과 상주 가중치 감소 |
| 코어 잔차 재사용 | `--core-reuse 1` | `--core-reuse 4` | `--core-reuse 6` | patch/head는 갱신하고 고비용 코어 빈도 감소 |
| 토큰 축소 | 끔 | 선택 | `--token-reduction` | 중간 블록에서 수평 비디오 토큰을 쌍으로 묶음 |
| 내부 캔버스 | 출력 크기 | 512 출력에 `384x384` | `320x320` | 작은 DiT/VAE 실행 뒤 vImage 업스케일 |

M5의 `--use-int8-row-fc2`는 FC2 행마다 활성화 스케일 하나와 전폭 TensorOps 곱을 쓴다. 그룹 int8보다 수치적으로 공격적이므로 선택 사항이다. 완전 디노이저 순전파가 약 2.6% 줄었고, 4스텝 여우/서퍼 비디오는 같은 피사체·환경·움직임을 유지했다(전체 비디오 SSIM 0.919/0.828). `--reuse`와 `--core-reuse`는 상호 배타적이며 레이어 축소는 어느 쪽과도 조합할 수 있다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A surfer riding inside a sharp blue ocean wave, one rider and one white board, realistic spray." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 45 --reuse 2 --token-reduction \
  -o outputs/surfer-fast.mp4
```

검증된 512 정사각형에서 토큰 축소는 `45 layers + reuse 2`의 디노이즈 시간을 16.69초에서 12.60초로 줄였다. 더 공격적인 프리뷰는 `--render-width 320 --render-height 320 --layers 40 --reuse 3`을 사용할 수 있으나 미세 디테일과 프레이밍이 달라질 수 있다. `--token-reduction`, `--layers 40`, `--reuse 3` 세 가지를 동시에 조합하면 색 띠, 윤곽선, 유령 팔다리가 관측됐으므로 피한다.

전체 속도 재사용 대신 타임스텝 의존 patch/output head를 매 전이마다 새로 유지하려면 다음처럼 실행한다.

```sh
./h3 --profile -d ./MiniMax-H3 -p "A surfer riding a blue ocean wave." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 45 --core-reuse 4 -o outputs/surfer-core-reuse.mp4
```

`--core-reuse 6`은 공격적 프리뷰에만 쓰며, 6보다 큰 값은 피사체 충실도가 떨어져 노출하지 않는다.

### 5. 해상도와 길이 선택하기

가로·세로는 각각 32의 배수이고 최소 32여야 하며 총 픽셀은 `768 * 1344`를 넘을 수 없다. 이는 기계적 한계이지 모든 작은 캔버스에서 품질을 보장한다는 뜻은 아니다. H3-Base는 768p 모델이다.

| 캔버스 | 현재 지침 |
|---|---|
| `512x512` | 여러 프롬프트로 반복 검증된 가장 안전한 개발 크기 |
| `768x768` | 검증된 고품질 정사각형, 비용이 큼 |
| `1344x768`, `768x1344` | 공개된 768p급 가로/세로 한계 |
| `1024x768`, `768x1024` | 유효한 4:3 및 3:4 캔버스 |
| 내부 `384x384` → 출력 `512x512` | 검증된 빠른 품질 지점 |
| 내부 `320x320` → 출력 `512x512` | 검증된 공격적 지점 |
| `256x256` | 저해상도 RoPE 적응이 자동 적용되는 네이티브 빠른 프리뷰 |

256 정사각형은 유효 공간 토큰 격자가 `8x8`뿐이므로 세부 묘사와 복잡한 구도가 제한된다. 정확히 256에서는 공간 RoPE 좌표를 자동으로 절반으로 줄인다. 동등성 검사 시 `--use-reference-rope`로 공개/MLX 좌표를 복원한다. 이 크기에서는 토큰 축소를 끈다. 128 정사각형은 조정 후에도 식별 가능한 피사체를 복원하지 못해 지원하지 않는다.

`--render-width`와 `--render-height`는 함께 지정하고 출력과 종횡비가 같아야 하며 출력보다 클 수 없다. H3는 24 fps이며 프레임 요청을 `5 + 17*n` 형태로 올림 정렬한다. `--seconds N`과 `--frames N`은 상호 배타적이다. 예를 들어 10초는 243프레임(10.125초)이 된다.

| 프레임 | 대략적인 길이 |
|---:|---:|
| 22 | 0.917초 |
| 39 | 1.625초 |
| 56 | 2.333초 |
| 107 | 4.458초 |
| 243 | 10.125초 |
| 362 | 15.083초 |

짧은 클립은 개발에 유용하고 공개 워크플로는 약 4~15초 비디오를 의도한다. 23프레임 요청은 임의 형태가 아니라 39프레임으로 올림된다.

### 6. 프롬프트 개선하기

짧은 프롬프트도 되지만 공개 시스템은 Context-IR 스타일 설명을 기대한다. 피사체, 행동, 환경, 카메라, 조명/스타일, 원하는 소리를 명시한다.

```text
Scene: a single red fox in a snow-covered pine forest at dawn.
Action: the fox walks steadily left to right and looks toward the camera once.
Camera: medium-height lateral tracking shot, 50 mm lens, stable framing.
Look: photorealistic fur, cold blue ambient light, warm sunrise rim light.
Audio: soft footsteps in snow, light wind through pine branches, no music.
```

정체성과 객체 수가 중요하면 명시한다. `--seed N`은 네이티브 난수 스트림을 제어하며 기본값은 42다. 옵션 비교 시 프롬프트, 시드, 해상도, 프레임 수, 스텝 수를 같게 유지한다.

### 7. 프레임 미리보기와 성능 진단

- `--show`: 매 디노이징 전이 후 대표 프레임과 완성된 전체 프레임을 표시한다.
- `--frames-dir DIR`: 최종 콜백 프레임을 PPM으로 쓴다. 중간 프리뷰는 저장하지 않는다.
- `-o ''`: MP4 인코딩을 끈다. FFmpeg가 없을 때 `--frames-dir`와 조합한다.
- `--profile`: 벽시계 시간, Metal 인코딩/대기 시간, 최대 라이브 텐서 저장 공간, 누적 할당, 디스패치 수를 출력한다.

```sh
./h3 --profile -d ./MiniMax-H3 -p "A hummingbird hovering over red flowers." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 45 --reuse 2 --frames-dir outputs/hummingbird-frames -o ''
```

### 8. 이미지·비디오·오디오 참조 추가하기

첫·마지막 프레임 앵커는 FL2VA 경로를 선택한다.

```sh
./h3 -d ./MiniMax-H3 -p "The fox keeps walking through the snow." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 45 --reuse 2 \
  --first-frame fox.png --last-frame fox-later.png \
  -o outputs/fox-anchored.mp4
```

순서가 있는 참조는 별도 Ref2VA 체크포인트를 선택한다.

```sh
# 이미지 참조
./h3 -d ./MiniMax-H3 -p "Use the animal and setting in the reference." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --ref-image fox.png -o outputs/fox-reference.mp4

# 사운드를 무시하는 비디오 참조
./h3 -d ./MiniMax-H3 -p "Continue the motion in this clip." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --ref-silent-video fox.mp4 -o outputs/fox-video-reference.mp4

# 내장 오디오를 보존하는 비디오 참조
./h3 -d ./MiniMax-H3 -p "Continue this audiovisual scene." \
  --width 512 --height 512 --frames 56 --steps 20 \
  --ref-video fox-with-audio.mp4 -o outputs/fox-video-audio.mp4

# 비디오 사운드트랙 교체
./h3 -d ./MiniMax-H3 -p "Continue the scene with the supplied music." \
  --width 512 --height 512 --frames 56 --steps 20 \
  --ref-video-audio silent-fox.mp4 replacement.wav \
  -o outputs/fox-replaced-audio.mp4

# 이미지와 독립 오디오 참조
./h3 -d ./MiniMax-H3 -p "Use the animal and music from the references." \
  --width 512 --height 512 --frames 56 --steps 20 \
  --ref-image fox.png --ref-audio music.wav \
  -o outputs/fox-image-audio.mp4
```

참조 플래그는 반복할 수 있고 CLI 순서가 보존된다. 독립 오디오는 이미지나 비디오 참조와 함께 써야 한다. 오디오 참조는 2~15초, 최대 3개이며 디코딩된 총길이는 15초로 제한된다.

## 테스트와 실행 요구 사항

```sh
make test
make parity
```

`make test`는 결정론적 호스트 테스트를 실행한다. 무시된 MLX fixture가 `misc/fixtures/`에 있으면 Metal 소스를 런타임 컴파일하고 완전한 toy H3 블록을 이름이 있는 MLX 출력과 비교한다. F32 진단 경로와 운영 BF16 저장 경로를 모두 검사한다. `make parity`는 Metal/MLX 동등성 검사만 실행한다.

미디어 입력과 MP4 출력에는 FFmpeg와 FFprobe가 `PATH`에 있어야 한다. `H3_FFMPEG`, `H3_FFPROBE`로 실행 파일을 직접 지정할 수도 있다. RGB24와 32 kHz 스테레오 F32 PCM은 동시 파이프로 전달되며 중간 비압축 미디어 파일을 만들지 않는다.

## 구현 및 성능 참고 사항

이 절은 튜토리얼 프리셋의 내부 구현과 정확한 A/B 진단용 환경 변수를 설명한다.

### 샘플러와 DiT 제어

기본 샘플러는 공개된 이동(shifted) 비디오/오디오 스케줄을 쓴다. `--steps`는 디노이징 패스 수이며 마지막 뒤에 종단 0을 붙인다. 전체 디노이저 재사용은 첫·마지막 및 지정 간격만 평가한 후 건너뛴 비디오/오디오 속도를 각 스케줄에서 외삽한다. 매우 적은 스텝에서는 `--reuse 1`을 유지한다.

저예산 경로에서는 여러 tail-heavy 스케줄보다 공개 선형 기본 격자가 우수했다. tail에 치우친 후보는 피사체를 선명하게 만들기도 했지만 움직임을 망치거나 반복 직조 배경을 남겼다. 레이어 축소는 체크포인트의 실제 AdaLN gate를 순위화하되 중요한 첫·마지막 블록을 보호한다. 코어 재사용은 이전 전체 트랜스포머 잔차를 유지하며 patch projection과 시간 인식 head를 갱신하고 전체 속도 재사용과는 함께 쓸 수 없다.

### 정확한 DiT 융합

활성 DiT 블록은 attention residual gate와 다음 MLP AdaLN을 융합한다. BF16 잔차 반올림은 그대로 유지하면서 같은 행을 threadgroup 메모리에 두어 디스패치와 전역 재읽기를 줄인다. `H3_DISABLE_FUSED_GATE_ADALN=1`, `H3_DISABLE_FUSED_CROSS_BLOCK_ADALN=1`은 2커널 오라클을 복원한다. 마지막 오디오/비디오 AdaLN은 residual stream 오프셋에 직접 바인딩해 512x512에서 18.8 MiB, 864급에서 29.4 MiB scratch를 피한다. `H3_DISABLE_FUSED_FINAL_SLICE=1`, `H3_DISABLE_FUSED_FINAL_HEAD=1`로 각각 기준 경로를 복원할 수 있다. 두 최적화의 총 절감은 37.5/58.9 MiB다.

### 토큰 축소 내부 동작

`--token-reduction`은 독립적인 공격적 DiT 모드다. 블록 3 뒤에서 수평 목표 비디오 토큰을 쌍으로 묶되 텍스트·오디오·조건·참조 토큰은 정확히 유지한다. 전체 해상도 상태는 bypass로 보존하며 초기 잡음 평가 10회에서는 블록 40 전에, 이후에는 블록 30 전에 복원한다. 각 토큰은 원래 값에 쌍이 학습한 업데이트를 더해 돌아온다.

512x512x22, 19회 순전파 M5 Max A/B에서 디노이즈 시간은 39.13→28.06초(28.3%), 최종 비디오/오디오 latent 상대 L2는 5.56%/15.14%였다. 구도를 바꿀 수 있어 기본값이 아니다. `H3_TOKEN_REDUCTION_BLOCKS`, `H3_TOKEN_REDUCTION_EARLY`, `H3_DISABLE_TOKEN_REDUCTION`, `H3_DISABLE_FUSED_TOKEN_POOL_ADALN`, `H3_DISABLE_FUSED_TOKEN_ADALN`으로 범위와 오라클을 제어한다. `--layers 45 --reuse 2`와의 조합은 16.69→12.60초였지만 `--layers 40 --reuse 3`까지 동시에 쓰면 품질 문제가 발생했다.

### 내부 캔버스와 비디오 VAE

`--render-width/--render-height`는 같은 종횡비의 작은 캔버스에서 모델과 VAE를 실행한 뒤 콜백·터미널·인코딩 전에 vImage로 업스케일한다. 384→512 측정에서 M5 DiT는 33%, video VAE는 18% 감소했다. 512 출력에서는 384가 빠른 품질 지점, 320이 공격적 지점이다. 비디오 VAE는 형상에 따라 256~320픽셀 타일을 자동 선택하고 `H3_VAE_TILE_PIXELS=256`으로 보수적 계획을 복원할 수 있다.

### 가중치 상주와 스트리밍 프롬프트 인코딩

M5급 GPU는 37 GiB 트랜스포머 가중치를 익명 공유 버퍼로 복사하는 대신 safetensor shard에서 직접 매핑해 파일 기반·회수 가능 상태로 둔다. M3는 복사 버퍼 경로를 쓴다. `H3_ZERO_COPY_WEIGHTS=0`으로 M5 선택을 끈다. Qwen 텍스트 인코더는 Metal 실행과 겹치도록 이후 레이어 버퍼의 작은 ring을 I/O 작업자 8개로 채운다. `H3_QWEN_PREFETCH=0..8`, `H3_QWEN_PREFETCH_DEPTH=1..6`으로 조절한다.

`--ssd-streaming`은 DiT에 대한 더 공격적인 상주 모드다. 정규화 가중치만 상주시킨 뒤 BF16 행렬 슬롯 두 개를 번갈아 사용한다. 내부 SSD에서 약 13~14.6 GiB/s가 측정됐으며 `H3_PROFILE=1`이 읽기량·처리량·GPU 작업으로 가려지지 않은 대기 시간을 보고한다.

### Metal 4와 TensorOps 경로

M5는 시퀀스 길이 2,048 이하에서 DiT QKV와 attention-output projection에 네이티브 BF16 Metal 4/TensorOps를 자동 사용한다. Morton 스케줄이 Q/K/V를 head-major 입력으로 직접 보내 세 번의 MPSGraph transpose를 피하며 이식 경로와 바이트가 같다. 512x512 50블록 순전파가 약 2% 개선됐다. 2,049~3,072행은 두 offset dispatch를 쓰며, 더 큰 시퀀스는 MPSGraph를 유지한다. `H3_NAX=0`은 진단을 위해 TensorOps를 끈다.

`H3_NAX=1`은 더 넓은 네이티브 BF16 선형 경로를 강제한다. `H3_NAX=mlp`는 FC1 gate/up TensorOps tile에서 SwiGLU를 적용한 뒤 FC2까지 TensorOps에 두는 전문 경로다. OS GPU 스택에 따라 전체 순전파가 퇴행할 수 있어 opt-in이다. `H3_DISABLE_NAX_MLP=1`로 같은 컨텍스트에서 MPSGraph MLP와 비교한다.

### 전문화된 projection 커널

좁은 DiT 오디오/비디오 출력 head는 작은 F32 가중치를 한 번 BF16으로 바꾸고 16x16 tiled linear를 쓴다. 320 렌더 형상에서 M3 Max 2.30배, M5 Max 1.83배였고 상대 L2는 `8.64e-4`였다. `H3_DIT_F32_FINAL=1`은 F32 기준 head를 복원한다. F32 비디오 `96->5376`, 오디오 `32->5376` patch projection은 전용 16x16 협력 tile을 쓰며 최종 cast와 packed hidden stream 기록을 융합해 scratch와 blit을 줄인다. `H3_DISABLE_FUSED_PATCH_CAST`, `H3_SCALAR_PATCH`, `H3_DISABLE_FUSED_PATCH_PACK`으로 각각 기준 경로를 선택한다.

### 스케줄링과 활성화 메모리

DiT 코어는 두 Metal command buffer로 나뉘어 GPU의 앞부분 실행과 CPU의 뒷부분 인코딩을 겹친다. M5는 깊이 60% 분할, M3는 검증된 30/50만 자동 선택한다. `H3_DIT_COMMAND_BLOCKS=0`은 단일 버퍼를 복원하며 1~50으로 분할을 덮어쓴다.

QKV, attention, 정규화 MLP 입력, MLP 출력 arena는 실제 수명에 맞춰 alias되어 512급 61.25 MiB, 864급 99.63 MiB를 줄인다. `H3_DISABLE_DIT_ACTIVATION_ALIAS=1`로 별도 버퍼를 복원한다. 불변 가중치의 MPSGraph wrapper 캐시는 `H3_DISABLE_GRAPH_DATA_CACHE=1`로 끈다. M3의 command wrapper 재사용은 `H3_REUSE_MPS_COMMAND=0/1`로 제어한다. M5 GPU sampler는 약 136 MB의 768p 호스트 임시 상태를 줄이며 `H3_CPU_SAMPLER`, `H3_GPU_SAMPLER`, `H3_GPU_SAMPLER_WINDOW`로 진단한다.

### 체크포인트 배치와 미디어 파이프라인

공개 체크포인트의 DiT QKV 행은 attention head별로 interleave되어 있다. 네이티브 Metal은 fused QK normalization/RoPE에서 이 배치를 직접 소비해 transpose와 추가 RAM을 피한다.

공개 생성 경로는 joint audio latent를 네이티브 BigVGAN/AudioVAE로 스트리밍 디코딩하고 동기화된 H.264와 32 kHz 스테레오 AAC를 쓴다. 네이티브 waveform은 수정된 MLX 오라클과 상대 L2 `6.94e-5`로 일치한다. FL2VA는 visual VAE, Qwen3-VL vision tower, 3-deepstack multimodal presentation을 사용한다. Ref2VA는 순서가 있는 `<Picture N>`/`<Video N>` 표현과 이미지·비디오·오디오 인코딩을 사용한다. 참조 오디오는 32 kHz 스테레오 F32로 디코딩하고 AudioVAE posterior mean으로 인코딩하며 0.999 clean latent + 0.001 seeded noise로 섞는다. 네이티브 오디오 인코더는 2초 스테레오 fixture에서 수정된 MLX 오라클과 상대 L2 `3.59e-6`였다.

### 프로파일링과 진단 경로

`--profile`은 Metal 단계별 벽시계 시간, CPU command encoding, commit-to-fence 대기, root-command GPU timestamp, 최대 라이브 텐서, 누적 할당, dispatch 수를 출력한다. M5의 기본 네이티브 int8 MLP는 활성화를 동적 양자화하고 출력 채널별 weight scale을 사용한다. 고정 50레이어·19전이·512x512에서 BF16 MPS 36.30초 대비 int8 25.80초였고 최대 텐서 저장 공간은 36.4 GiB→25.9 GiB였다.

int8 QKV는 같은 실험의 디노이징을 25.80→19.32초로 줄였고, 이어지는 int8 attention-output projection은 완전 순전파를 4.5~5.5% 더 개선했다. SDPA 결과의 head-major 상태를 직접 gather/quantize하고, gated AdaLN에 QKV/MLP 활성화 양자화를 융합하며, Q/K RMS와 RoPE를 int8 QKV tile에 넣는다. 각 단계는 다음 진단 옵션으로 기준 경로를 복원한다.

- `--use-slower-bf16-mlp`
- `--use-slower-bf16-qkv`
- `--use-slower-bf16-attention-output`
- `--use-slower-row-major-attention-output`
- `--use-slower-unfused-int8-inputs`
- `--use-slower-unfused-qkv-rope`
- `--use-slower-scalar-qkv-rms`
- `--use-slower-uncached-int8-scales`
- `--use-slower-dynamic-fc1-k`
- `--use-slower-grouped-quantizer`

```sh
./h3 --profile -d ./MiniMax-H3 \
  -p "A red fox walks through fresh snow." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 50 --reuse 1 -o outputs/fox-int8.mp4
```

네이티브 기준선은 원본 `FL2VA/`와 `Ref2VA/` 체크포인트 트리를 대상으로 한다. 모델 단계를 따로 로드하고 해제하므로 33B 트랜스포머, Qwen 인코더, 디코더가 통합 메모리에 동시에 존재하지 않아도 된다.

## 더 학습하기

설치 점검, C API, 내부 구조, 성능 실험 방법은 [한국어 학습 가이드](guide/README.md)를 따른다.
