# firmware/hal

## OVERVIEW
FM-1(JieLi AC79/pi32v2) 하드웨어 추상화 계층. 헤더 15개 + 어셈블리 2개(약 1.6k LOC). 점수 10이고, 레지스터·ISR을 다루는 별도 영역이라 지침 파일을 둡니다.

## 구조
- 헤더 전용 HAL입니다. `.c`가 없으며 `firmware/src/felucca.c`가 맨 앞에서 `fm1_time.h`, `fm1_sys.h`, `fm1_irq.h`, `fm1_guard.h`, `fm1_input.h`, `fm1_timer.h`, `fm1_audio.h`, `fm1_adc.h`, ... 순서로 include합니다.
- 어셈블리: `fm1_isr.S`(`FM1_ISR` 매크로로 상태 저장/복구, csync, rti), `fm1_vec.S`(벡터 테이블).
- 링크/부트 파일은 상위 디렉터리에 있습니다: `firmware/app.ld`, `firmware/crt0.S`. 업데이트 로더는 `firmware/loader/`(`loader.c`, `ldr_core.c`, `crt0_ldr.S`, `loader.ld`)에 있습니다.

## WHERE TO LOOK
| 작업 | 파일 | 핵심 심볼 |
|------|------|-----------|
| 인터럽트 등록·마스크 | `fm1_irq.h` | `fm1_irq_init`, `fm1_irq_attach`, `fm1_irq_off/on`, `FM1_IRQ_*`(ALNK0, TIMER5, UART1, SARADC ...), `fm1_crash_t` |
| 부트·리셋·워치독 | `fm1_sys.h` | `fm1_wdt_*`, `fm1_reset_reason`, `fm1_reboot`, `fm1_enter_uboot`, `fm1_p33_*` |
| I2S 오디오 DMA | `fm1_audio.h` | `fm1_audio_init(buf, half_words, isr, prio)`, `FM1_AUDIO_HALF`(이중 버퍼 절반 표시) |
| 시간·타이머 | `fm1_time.h`, `fm1_timer.h` | ms 카운터, delay |
| 입력·ADC·GPIO | `fm1_input.h`, `fm1_adc.h`, `fm1_gpio.h` | `FM1_PR(port, field)` 레지스터 필드 매크로 |
| 플래시·XIP | `fm1_flash.h`, `fm1_xip.h` | 저장소·OTA가 사용 |
| UART MIDI(TRS)·USB·LCD | `fm1_uart.h`, `fm1_usb.h`, `fm1_lcd_hw.h` | |
| 클럭·보호 | `fm1_cc.h`, `fm1_guard.h` | |

## CONVENTIONS
- 모든 공개 이름은 `fm1_*`(함수·구조체) / `FM1_*`(매크로·enum) 접두사를 씁니다. firmware/src에서 `fm1_` 식별자 사용이 300회 이상이므로 이름을 바꾸면 영향이 넓습니다.
- 인라인 함수는 `FM1_INLINE`(static inline)으로 선언합니다. 레지스터 접근은 매크로 하나로 감싸고 순서를 바꾸지 않습니다.
- `fm1_irq_init`은 EMU_CON bit 2(나눗셈 0 트랩)를 명시적으로 지우고, 다른 비트·예외 설정·벡터·branch trace는 보존합니다(upstream Felucca #61/#111 대응). 이 동작은 `tests/irq_init_test.c`가 RAM 기반 MMIO로 검증합니다. 트랩을 끈다고 산술 오류가 고쳐지지는 않습니다.
- 파일마다 SPDX `GPL-3.0-only`와 Copyright (Leo Kuroshita, Hügelton Instruments) 헤더를 유지합니다.

## ANTI-PATTERNS
- 호스트 테스트(`-Ifirmware/hal` 스텁·더블)가 통과했다고 레지스터 동작이 맞다고 결론짓기. 실제 타이밍·부트 검증은 타깃과 기기에서만 할 수 있습니다.
- ISR 우선순위나 벡터를 바꾸면서 `fm1_isr.S`/`fm1_vec.S`를 함께 갱신하지 않기.
- HAL에 `.c` 구현 파일을 추가해 단일 번역 단위 구조를 깨기.
