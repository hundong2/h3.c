# C 실습 예제

## 1. 기본값 검사(모델 불필요)

학습 목표는 공개 헤더의 기본값과 형상 제한을 읽고 오류를 생성 전에 차단하는 것이다.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic inspect_defaults.c -o inspect_defaults
./inspect_defaults
```

예상 출력에는 버전, 기본 `864x480`, 56프레임, 20스텝과 세 개의 캔버스 검사 결과가 포함된다. 이 파일은 `h3.h`만 include하므로 macOS가 아닌 환경에서도 C 컴파일러로 실행할 수 있다.

## 2. 공개 API로 생성(macOS + 모델 필요)

저장소 루트에서 먼저 `make`로 `libh3.a`를 만든 뒤 실행한다.

```sh
clang -std=c11 -I. guide/examples/generate_video.c libh3.a \
  -framework Foundation -framework Metal \
  -framework MetalPerformanceShaders -framework MetalPerformanceShadersGraph \
  -framework Accelerate -licucore -lm -o guide/examples/generate_video

mkdir -p outputs
./guide/examples/generate_video ./MiniMax-H3 outputs/api-example.mp4
```

실행 전 요구 사항은 Apple Silicon, 완전한 모델 스냅샷, FFmpeg/FFprobe다. 예제는 256x256, 22프레임, 4스텝으로 비용을 낮추고 진행 콜백을 출력한다. 종료 코드 0과 출력 MP4 생성을 확인한다.

## 연습 문제

1. `inspect_defaults.c`에 프레임 정렬 함수 `5 + 17*n`을 구현하고 23이 39가 되는지 확인한다.
2. `generate_video.c`에 `on_frame` 콜백을 추가해 프레임 번호와 stride를 출력한다.
3. `steps=4`, `7`, `20`을 같은 시드로 실행해 wall time과 결과 차이를 기록한다.
4. 512 출력에 내부 384 렌더를 설정하고 기준 512 렌더와 비교한다.
