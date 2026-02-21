# CPU Core Instrument 🎵

CPU 논리 코어 사용량을 읽어 코어별로 음을 생성하는 실시간 CLI 악기입니다.

각 코어의 사용률이 높을수록 더 높은 음이 납니다. 터미널에서 실시간으로 코어별 사용량 그래프도 표시됩니다.

## Features

- Mach API 기반 코어별 CPU 사용률 샘플링
- EMA 스무딩 + 바닥값 처리
- 마이너 펜타토닉 기반 피치 매핑
- Core Audio AudioUnit 저지연 출력
- 터미널 코어별 실시간 텍스트 그래프

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/cpu_core_instrument
```

볼륨을 지정하려면 실행 시 옵션을 넘길 수 있습니다.

```bash
./build/cpu_core_instrument --volume 0.8
# 또는
./build/cpu_core_instrument -v 1.2
```

- 범위: `0.0` ~ `2.0`
- 기본값: `1.0`

실행 중 `Ctrl+C`로 종료합니다.

## macOS Notes

- 오디오 엔진은 기본 출력 디바이스를 사용합니다.
- 초기 MVP는 논리 코어 단위 매핑입니다.
- 실시간 오디오 콜백에서는 메모리 할당/락을 피하는 구조를 사용합니다.
