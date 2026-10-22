# Graph Report - Dash  (2026-10-22)

## Corpus Check
- 97 files · ~836,709 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1225 nodes · 2632 edges · 66 communities (64 shown, 2 thin omitted)
- Extraction: 76% EXTRACTED · 24% INFERRED · 0% AMBIGUOUS · INFERRED: 632 edges (avg confidence: 0.8)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `3016defc`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- obd_rx_poll
- Чистовая сборка: из макетки в устройство
- Error_Handler
- cluster_ui.c
- test_lv_port_disp.c
- Colorado 2.8 Diesel Cluster v2 Mockup
- fault.c
- fdcan_obd.c
- stm32h7xx_hal_timebase_tim.c
- SNIFF page (STATE + ANALOG)
- test_alert.c
- test_fdcan_obd.c
- stdint
- Architecture
- lv_port_disp.c
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- stm32h7xx_it.c
- obd_poll_tick
- FDCAN1_IT0_IRQHandler
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- build.py
- main.c
- tim.c
- test_gps.c
- test_can_sniff.c
- test_input.c
- GPS-скорость и замер разгона («свой Dragy»)
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- test_st7735_status.c
- obd_init
- CLAUDE.md
- Findings
- main
- Экран: качество картинки и скорость
- fault_record
- build-hardware.md
- Плата-носитель 7×9 см: раскладка и провода
- In-vehicle install — from bench demo to a live car
- obd_pid_supported
- GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98
- string
- SNIFF capture — selector / PRNDL search
- lv_port_disp_init
- Первая прошивка stage-c: чек-лист
- HAL_StatusTypeDef
- Статус исправлений (2026-10-09)
- HAL_GPIO_Init
- mock_lv_port_disp/lvgl.h
- st7735_status.c
- assembly_steps.py
- build_drive
- Dash — обзор проекта для ревью
- wiring_overview.py
- Oil pressure: where it comes from, and how to check it
- Сборка платы-носителя по шагам: что куда паять
- main
- carrier_board.py
- gps.c
- gps_nmea.c
- Жгут: провода от коробки к машине, экрану, кнопкам и GPS
- spi.c
- HAL_StatusTypeDef
- HAL_GPIO_Init

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 89 edges
2. `cluster_ui_refresh()` - 43 edges
3. `st7735_status_init()` - 35 edges
4. `obd_poll_tick()` - 34 edges
5. `near()` - 27 edges
6. `AppMain_Run()` - 25 edges
7. `build_drive()` - 20 edges
8. `req_mode01()` - 20 edges
9. `build_diag()` - 18 edges
10. `alerts_state()` - 17 edges

## Surprising Connections (you probably didn't know these)
- `t_dlc15_classic_frame()` --calls--> `obd_rx_poll()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Src/fdcan_obd.c
- `t_sf_len15_rejected()` --calls--> `obd_rx_poll()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Src/fdcan_obd.c
- `t_sf_short_dlc_rejected()` --calls--> `obd_rx_poll()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Src/fdcan_obd.c
- `t_start_retried_after_failure()` --calls--> `obd_rx_poll()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Src/fdcan_obd.c
- `t_unchanged_value_restamps()` --calls--> `obd_rx_poll()`  [INFERRED]
  tests/host/test_fdcan_obd.c → Core/Src/fdcan_obd.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (66 total, 2 thin omitted)

### Community 0 - "obd_rx_poll"
Cohesion: 0.07
Nodes (35): req_oilp(), IRQn_Type, cap_scenario(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), last_tx_is(), main() (+27 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "Error_Handler"
Cohesion: 0.50
Nodes (4): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), fdcan

### Community 3 - "cluster_ui.c"
Cohesion: 0.16
Nodes (30): lv_color_t, metric_key_t, metric_state_t, obd_data_t, cat(), cluster_ui_next_page(), cluster_ui_prev_page(), cluster_ui_refresh() (+22 more)

### Community 4 - "test_lv_port_disp.c"
Cohesion: 0.19
Nodes (19): lv_port_disp_service(), IRQn_Type, dma_done(), flush(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), t_boot_errors_then_good_dma(), t_dma_refused_fallback_fails() (+11 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.14
Nodes (19): assert_text(), fault_assert_failed(), fault_init(), fault_record(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_selftest() (+11 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.13
Nodes (33): cluster_app_run(), obd_on_stamp(), obd_on_update(), can_send(), decode_mode01(), decode_mode22(), dispatch(), mil_attempt_failed() (+25 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "test_alert.c"
Cohesion: 0.15
Nodes (30): alert, alert_cfg_t, alert_t, metric_key_t, metric_state_t, metric_state(), alert_reset(), alert_step() (+22 more)

### Community 11 - "test_fdcan_obd.c"
Cohesion: 0.38
Nodes (7): obd_pid_support(), obd_pid_supported(), obd_pid_support_t, ps_run(), t_pidsup_gives_up_unanswered(), t_pidsup_map_from_any_reply(), t_pidsup_reads_advertised_chain()

### Community 12 - "stdint"
Cohesion: 0.09
Nodes (20): metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh(), policy_fresh(), policy_shown(), policy_sniff_allowed(), policy_summary() (+12 more)

### Community 13 - "Architecture"
Cohesion: 0.12
Nodes (27): can_sniff, cluster_app, cluster_ui, app_loop_max_ms(), AppMain_Init(), AppMain_Run(), backlight_set(), bl_active() (+19 more)

### Community 14 - "lv_port_disp.c"
Cohesion: 0.18
Nodes (24): lv_area_t, lv_display_t, SPI_HandleTypeDef, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb() (+16 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 16 - "stm32h7xx_it.c"
Cohesion: 0.15
Nodes (5): app_main, fault, main, stm32h7xx_it, task

### Community 17 - "obd_poll_tick"
Cohesion: 0.11
Nodes (23): can_sniff_is_active(), obd_poll_tick(), req_mode01(), req_mode22(), t_d03_nrc78_total_cap(), t_d03_nrc78_within_cap_completes(), t_foreign_ff_gets_no_fc(), t_foreign_sf_keeps_slot() (+15 more)

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.29
Nodes (12): FDCAN1_IT0_IRQHandler(), obd_can_health(), obd_health_t, irq_mode(), t_d05_arrival_check_across_wrap(), t_d05_late_arrival_is_not_an_answer(), t_d06_sniff_gets_rx_time(), t_health_check_masks_rx_irq() (+4 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "build.py"
Cohesion: 0.24
Nodes (16): nmea_checksum(), gps_hw_write(), nmea_result_t, feed_body(), feed_raw(), near(), t_bad_checksum_rejected(), t_checksum_known() (+8 more)

### Community 23 - "main.c"
Cohesion: 0.14
Nodes (21): bus(), cap(), check(), diode(), esc(), module(), Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per s, Resistor between two points on one axis; label beside the body. (+13 more)

### Community 24 - "tim.c"
Cohesion: 0.47
Nodes (5): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), tim

### Community 25 - "test_gps.c"
Cohesion: 0.15
Nodes (18): feed_gps(), gps_fix_fresh(), gps_state(), gps_state_t, math, stdio, main(), mod_emit() (+10 more)

### Community 27 - "test_can_sniff.c"
Cohesion: 0.12
Nodes (31): apply_filter(), can_sniff_candidates(), can_sniff_feed(), can_sniff_filter_error(), can_sniff_fps(), can_sniff_id_count(), can_sniff_movers(), can_sniff_reset() (+23 more)

### Community 28 - "test_input.c"
Cohesion: 0.24
Nodes (17): btn_out_t, btn_t, btn_step(), daynight_step(), daynight_t, exti_press(), hold_level(), reset() (+9 more)

### Community 29 - "GPS-скорость и замер разгона («свой Dragy»)"
Cohesion: 0.11
Nodes (18): 1. Зачем GPS, если есть OBD-скорость, 2. Аналоги, 3. Выбор GNSS-чипа, 4. Интеграция в текущую систему, 5. План прошивки и трудозатраты, 6. Подводные камни, 7. Резервное питание модуля: не нужно, 8. Железо: варианты и BOM (+10 more)

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "test_st7735_status.c"
Cohesion: 0.11
Nodes (40): st7735_status_init(), st7735_status_recoveries(), st7735_status_service(), st7735_status_spi_errors(), lv_lcd_flag_t, lv_st7735_send_cmd_cb_t, lv_st7735_send_color_cb_t, stdarg (+32 more)

### Community 32 - "obd_init"
Cohesion: 0.12
Nodes (28): FDCAN_HandleTypeDef, HAL_StatusTypeDef, cfg_ok(), HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), FDCAN_ProtocolStatusTypeDef (+20 more)

### Community 33 - "CLAUDE.md"
Cohesion: 0.22
Nodes (7): Architecture, Build / flash / debug, Git workflow: commit and push automatically, graphify, Hardware notes worth knowing before touching related code, Use graphify before and after non-trivial changes, What this is

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "main"
Cohesion: 0.28
Nodes (13): st7735_status_backlight(), st7735_status_key_dbg(), st7735_status_note(), st7735_status_set(), st_link_t, lv_label_set_text(), lv_label_set_text_fmt(), lv_lock() (+5 more)

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.13
Nodes (15): 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09), 7. Подводные камни (+7 more)

### Community 37 - "fault_record"
Cohesion: 0.15
Nodes (13): Backlight control module, Boot / programming, CAN transceiver module, Core board, External modules (as purchased), Hardware reference — WeAct MiniSTM32H743, Main 4" display module, Onboard 0.96" ST7735 status display (SPI4) (+5 more)

### Community 39 - "Плата-носитель 7×9 см: раскладка и провода"
Cohesion: 0.25
Nodes (8): 1. Распиновка гребёнок WeAct, 2. Гнёзда под WeAct: хватит двух 1×40, 3. Клеммники 3.81 мм на сетке 2.54: гнуть выводы, 4. Что где стоит, 5. Провода, 6. Проверки до установки WeAct, 7. Высота в коробке 140×82×38, Плата-носитель 7×9 см: раскладка и провода

### Community 40 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.17
Nodes (12): 1. Powering the board in the car, 2. Connecting to the CAN bus (OBD-II port), 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, ⚠️ Bus termination — the #1 thing to get right, In-vehicle install — from bench demo to a live car (+4 more)

### Community 42 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.29
Nodes (7): Added, awaiting on-car confirmation, Being probed on the car (standard diesel PIDs), Confirmed on this vehicle, GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources

### Community 43 - "string"
Cohesion: 0.12
Nodes (28): arrived_in_time(), obd_rx_poll(), past_txn_cap(), rxq_pop(), rxq_t, egt_reply(), egt_request(), near() (+20 more)

### Community 44 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.33
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 45 - "lv_port_disp_init"
Cohesion: 0.13
Nodes (19): lv_port_disp_init(), lv_display_flush_cb_t, lv_tick_get_cb_t, lv_display_t, lv_obj_t, lv_display_create(), lv_display_flush_ready(), lv_display_get_screen_active() (+11 more)

### Community 46 - "Первая прошивка stage-c: чек-лист"
Cohesion: 0.22
Nodes (9): 0. Какой образ шить, 1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS, 2. Образ DEMO: цвета и тревоги без машины, 3. Проверка сбросов (FAULT_TEST), 4. Когда придёт GPS (ATGM336H), 5. Когда будет плата-носитель, 6. Первый раз в машине, 7. Потом (+1 more)

### Community 47 - "HAL_StatusTypeDef"
Cohesion: 0.36
Nodes (8): SPI2_IRQHandler(), HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_Init(), HAL_SPI_IRQHandler(), HAL_SPI_Transmit(), HAL_SPI_Transmit_DMA()

### Community 48 - "Статус исправлений (2026-10-09)"
Cohesion: 0.40
Nodes (5): DMA1_Stream0_IRQHandler(), spi2_dma_init(), DMA_HandleTypeDef, HAL_DMA_Init(), HAL_DMA_IRQHandler()

### Community 49 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

### Community 50 - "mock_lv_port_disp/lvgl.h"
Cohesion: 0.67
Nodes (3): lv_area_t, lv_area_get_height(), lv_area_get_width()

### Community 51 - "st7735_status.c"
Cohesion: 0.25
Nodes (14): lv_display_t, st_broken(), st_cs_high(), st_cs_low(), st_dc_cmd(), st_dc_data(), st_hw_init(), st_send_cmd() (+6 more)

### Community 52 - "assembly_steps.py"
Cohesion: 0.05
Nodes (61): argparse, concurrent_futures, hashlib, importlib_util, os, pathlib, pymupdf, re (+53 more)

### Community 53 - "build_drive"
Cohesion: 0.38
Nodes (24): build_diag(), build_drive(), build_sniff(), build_stats(), build_strip(), lv_font_t, lv_obj_t, cluster_ui_build() (+16 more)

### Community 54 - "Dash — обзор проекта для ревью"
Cohesion: 0.07
Nodes (30): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 3. Архитектура, 4. Сборка, прошивка, инструменты, 5. Находки ревью, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору (+22 more)

### Community 55 - "wiring_overview.py"
Cohesion: 0.26
Nodes (17): speed_cal_band(), speed_cal_factor(), speed_cal_gps(), speed_cal_obd(), speed_cal_reset(), steady(), sc_band_t, drive() (+9 more)

### Community 56 - "Oil pressure: where it comes from, and how to check it"
Cohesion: 0.33
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 57 - "Сборка платы-носителя по шагам: что куда паять"
Cohesion: 0.20
Nodes (10): После всех шагов, Сборка платы-носителя по шагам: что куда паять, Шаг 1. Питание, Шаг 2. Шины, Шаг 3. Экран (X5), Шаг 4. Подсветка (2N7000 + Si4599), Шаг 5. CAN (X2), Шаг 6. GPS (X3) (+2 more)

### Community 58 - "main"
Cohesion: 0.26
Nodes (17): trip_stats_channel(), trip_stats_loop(), trip_stats_loop_max(), trip_stats_p95(), trip_stats_reset(), trip_stats_since(), trip_stats_update(), string (+9 more)

### Community 59 - "carrier_board.py"
Cohesion: 0.13
Nodes (17): metric_key_t, obd_data_t, obd_is_fresh(), obd_mil_fresh(), obd_sel_fresh(), oilp_standstill(), metric_key_t, obd_data_t (+9 more)

### Community 60 - "gps.c"
Cohesion: 0.21
Nodes (14): enter(), gps_init(), gps_poll(), gps_hw_getc(), gps_hw_init(), gps_hw_line_ms(), gps_hw_now_ms(), gps_hw_overruns() (+6 more)

### Community 61 - "gps_nmea.c"
Cohesion: 0.29
Nodes (15): angle(), nmea_result_t, dd(), hexval(), nmea_feed(), nmea_init(), num(), parse_gga() (+7 more)

### Community 62 - "Жгут: провода от коробки к машине, экрану, кнопкам и GPS"
Cohesion: 0.25
Nodes (8): X1 — питание, X2 — CAN, X3 — GPS, X4 — кнопки и габариты, X5 — экран, Жгут: провода от коробки к машине, экрану, кнопкам и GPS, Общие правила, Перед первым включением в машине

### Community 63 - "spi.c"
Cohesion: 0.15
Nodes (15): MX_FDCAN1_Init(), MX_GPIO_Init(), TIM_HandleTypeDef, Error_Handler(), HAL_TIM_PeriodElapsedCallback(), main(), MPU_Config(), SystemClock_Config() (+7 more)

### Community 64 - "HAL_StatusTypeDef"
Cohesion: 0.53
Nodes (6): HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_DeInit(), HAL_SPI_Init(), HAL_SPI_Transmit()

### Community 65 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

## Knowledge Gaps
- **204 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Build / flash / debug`, `Architecture`, `Use graphify before and after non-trivial changes` (+199 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **2 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `cluster_ui_refresh()` connect `cluster_ui.c` to `fdcan_obd.c`, `carrier_board.py`, `test_alert.c`, `test_fdcan_obd.c`, `stdint`, `Architecture`, `lv_port_disp.c`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `build_drive`, `test_gps.c`, `test_can_sniff.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.035) - this node is a cross-community bridge._
- **Why does `AppMain_Run()` connect `Architecture` to `main`, `test_lv_port_disp.c`, `fault.c`, `fdcan_obd.c`, `stdint`, `gps.c`, `test_input.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.020) - this node is a cross-community bridge._
- **Why does `obd_rx_poll()` connect `string` to `obd_init`, `obd_rx_poll`, `fdcan_obd.c`, `carrier_board.py`, `test_fdcan_obd.c`, `stdint`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_can_sniff.c`?**
  _High betweenness centrality (0.018) - this node is a cross-community bridge._
- **Are the 75 inferred relationships involving `obd_rx_poll()` (e.g. with `cluster_app_run()` and `can_sniff_feed()`) actually correct?**
  _`obd_rx_poll()` has 75 INFERRED edges - model-reasoned connections that need verification._
- **Are the 28 inferred relationships involving `cluster_ui_refresh()` (e.g. with `cluster_app_run()` and `policy_fresh()`) actually correct?**
  _`cluster_ui_refresh()` has 28 INFERRED edges - model-reasoned connections that need verification._
- **Are the 33 inferred relationships involving `st7735_status_init()` (e.g. with `AppMain_Init()` and `lv_color_black()`) actually correct?**
  _`st7735_status_init()` has 33 INFERRED edges - model-reasoned connections that need verification._
- **Are the 27 inferred relationships involving `obd_poll_tick()` (e.g. with `cluster_app_run()` and `can_sniff_is_active()`) actually correct?**
  _`obd_poll_tick()` has 27 INFERRED edges - model-reasoned connections that need verification._