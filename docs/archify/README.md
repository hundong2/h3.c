# h3.c Archify Architecture

작성일: 2026-09-03

## 목차

- [산출물](#산출물)
- [출처와 근거](#출처와-근거)
- [적용 프롬프트](#적용-프롬프트)
- [다이어그램 읽는 법](#다이어그램-읽는-법)
- [Unknowns](#unknowns)

## 산출물

- [architecture.json](architecture.json)
- [architecture.html](architecture.html)
- [architecture.visual-check.json](architecture.visual-check.json)
- [architecture.visual-check.html](architecture.visual-check.html)
- `architecture.visual-check.1440x900.light.png`
- `architecture.visual-check.1440x900.dark.png`
- `architecture.visual-check.2048x1320.light.png`
- `architecture.visual-check.2048x1320.dark.png`

## 출처와 근거

- Repository URL: https://github.com/hundong2/h3.c.git
- Checkout SHA: 4b861b1ff3e489be6a4853d2a10ac7784c5bc504
- README file: README_kor.md
- Evidence file: README_kor.md
- README title: h3-metal
- Dependency markers: Make
- Top-level directories: docs, guide, tests
- Category: Systems and runtimes

## 적용 프롬프트

~~~~text
Use Archify to create a high-level architecture diagram for h3.c.
Inspect repository evidence before finalizing the diagram. Use README,
dependency files, configuration files, entrypoint scripts, and tests as evidence.

Describe users, core components, primary runtime path, external dependencies,
and repository boundaries. Keep one obvious primary path across 8-12 components.
Mark unknown model weights, datasets, API keys, hardware, or hosted services
instead of inventing them.

Optimize the diagram for code understanding: show where configuration enters,
where data is loaded, where the main model or engine runs, where evaluation or
UI rendering happens, and where outputs are written or displayed.
~~~~

## 다이어그램 읽는 법

주 경로는 `Developer / learner -> README and docs -> Setup and deps -> Entry points -> Runtime / systems core -> Binaries / services`이다.

상단의 `Inputs and config`는 설정 파일, CLI 옵션, 데이터 입력이 어디서 들어오는지 확인하는 위치다. 하단의 `Tests / evaluation`은 동작을 검증하는 경로이며, `External assets`는 모델 가중치, 데이터셋, API 키, 하드웨어 요구사항처럼 레포별로 확인해야 하는 실행 전제다.

## Unknowns

- 이 다이어그램은 README, dependency marker, top-level directory를 근거로 한 1차 구조도다.
- 실제 call graph, 모델 내부 연산, 배포 토폴로지는 레포별 세부 분석으로 따로 내려가야 한다.
- weights, datasets, credentials, hosted APIs, GPU/OS 요구사항은 실행 전에 원문 README와 설정 파일을 다시 확인해야 한다.