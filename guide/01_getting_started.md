# 01. 설치와 첫 실행

## 1. 지원 환경

이 저장소는 범용 C 프로젝트가 아니라 Apple Silicon용 Metal 구현이다.

- Apple Silicon Mac(M3/M5 경로가 집중 검증됨)
- macOS와 Command Line Tools 또는 Xcode의 `clang`, `make`
- MiniMax-H3 Hugging Face 모델 스냅샷
- 미디어 입력·MP4 출력용 `ffmpeg`, `ffprobe`
- 충분한 통합 메모리와 디스크 공간

Linux/Windows에서는 가이드의 순수 C 설정 예제는 실행할 수 있지만 본체는 Apple framework와 Objective-C/Metal 때문에 빌드할 수 없다.

## 2. 도구 확인

```sh
xcode-select -p
clang --version
make --version
ffmpeg -version
ffprobe -version
```

Homebrew를 사용한다면 FFmpeg는 `brew install ffmpeg`로 설치할 수 있다. 실행 파일이 `PATH`에 없으면 다음 환경 변수로 명시한다.

```sh
export H3_FFMPEG=/absolute/path/to/ffmpeg
export H3_FFPROBE=/absolute/path/to/ffprobe
```

## 3. 모델 배치와 빌드

모델 디렉터리를 저장소 루트의 `MiniMax-H3/`에 두거나 `-d`로 실제 경로를 전달한다. 모델 파일은 크므로 Git에 추가하지 않는다.

```sh
make -j8
mkdir -p outputs
./h3 --info -d ./MiniMax-H3
```

`--info`에서 실패하면 다음 순서로 확인한다.

1. `-d` 경로가 스냅샷 루트인지 확인한다.
2. FL2VA/Ref2VA 및 encoder/VAE shard가 완전한지 확인한다.
3. 선택된 Metal 장치와 권장 working set을 확인한다.
4. 모델 다운로드가 덜 끝난 파일이나 Git LFS pointer가 없는지 확인한다.

## 4. 첫 생성

비용을 낮춰 파이프라인부터 검증한다.

```sh
./h3 --profile \
  -d ./MiniMax-H3 \
  -p "A paper boat floating on a calm pond, static camera, soft morning light, gentle water sounds, no music." \
  --width 256 --height 256 \
  --frames 22 --steps 4 --layers 50 --reuse 1 \
  -o outputs/first.mp4
```

256 프리뷰가 성공하면 512의 균형 프리셋으로 이동한다.

```sh
./h3 --profile -d ./MiniMax-H3 \
  -p "A paper boat floating on a calm pond, static camera, soft morning light, gentle water sounds, no music." \
  --width 512 --height 512 --frames 22 --steps 20 \
  --layers 45 --reuse 2 -o outputs/balanced.mp4
```

## 5. 환경 변수는 필수인가?

일반 실행에 필수인 별도 h3 환경 변수는 없다. 모델은 `-d`, 생성 옵션은 CLI로 넘긴다. 단, FFmpeg가 `PATH`에 없을 때 `H3_FFMPEG`/`H3_FFPROBE`가 필요하다. `H3_PROFILE`, `H3_NAX`, `H3_QWEN_PREFETCH` 같은 변수는 성능 실험이나 진단용이므로 원인을 분리할 때만 사용한다.

## 6. 모델 없이 하는 첫 C 실습

```sh
cd guide/examples
cc -std=c11 -Wall -Wextra -Wpedantic inspect_defaults.c -o inspect_defaults
./inspect_defaults
```

이 예제는 `h3.h`만 읽으므로 모델과 Metal이 없어도 기본값과 캔버스 불변식을 확인한다. 다음은 [핵심 개념과 C API](02_core_concepts.md)다.
