# Graph Report - Dash  (2026-10-28)

## Corpus Check
- 116 files · ~898,599 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 5, .ld 2, .ioc 1)

## Summary
- 1535 nodes · 3597 edges · 78 communities (73 shown, 5 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 975 edges (avg confidence: 0.86)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `3168a6d2`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- obd_rx_poll
- Чистовая сборка: из макетки в устройство
- string
- test_can_sniff.c
- lv_port_disp.c
- Colorado 2.8 Diesel Cluster v2 Mockup
- fault.c
- fdcan_obd.c
- stm32h7xx_hal_timebase_tim.c
- SNIFF page (STATE + ANALOG)
- test_alert.c
- test_lv_port_disp.c
- stdint
- cluster_ui_refresh
- obd_poll_tick
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- stm32h7xx_it.c
- Src/main.c
- irq_mode
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- test_nvlog.c
- assembly_steps.py
- tim.c
- test_gps.c
- HAL_GetTick
- test_input.c
- fault_record
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- cluster_ui.c
- obd_init
- CLAUDE.md
- Findings
- build.py
- Экран: качество картинки и скорость
- Hardware reference — WeAct MiniSTM32H743
- build-hardware.md
- Плата-носитель 7×9 см: раскладка и провода
- In-vehicle install — from bench demo to a live car
- test_odo.c
- GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98
- wiring_overview.py
- HAL_FDCAN_Init
- ui_render.py
- Первая прошивка stage-c: чек-лист
- obd_on_update
- 2. Connecting to the CAN bus (OBD-II port)
- Architecture
- txn_failed
- AppMain_Run
- carrier_template.py
- main.h
- Dash — обзор проекта для ревью
- carrier_board.py
- oilp_standstill
- Сборка платы-носителя по шагам: что куда паять
- harness_tree.py
- test_perf.c
- req_oilp
- 3. Архитектура
- Жгут: провода от коробки к машине, экрану, кнопкам и GPS
- Error_Handler
- Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру
- Oil pressure: where it comes from, and how to check it
- main
- SNIFF capture — selector / PRNDL search
- HAL_StatusTypeDef
- ui_render/main.c
- stubs.c
- cluster_policy.h
- HAL_GPIO_Init
- math
- lv_area_get_width
- app_main.c
- btn_tap
- test_fdcan_obd.c

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 104 edges
2. `cluster_ui_refresh()` - 56 edges
3. `obd_poll_tick()` - 48 edges
4. `Architecture` - 43 edges
5. `st7735_status_init()` - 36 edges
6. `HAL_GetTick()` - 31 edges
7. `near()` - 30 edges
8. `AppMain_Run()` - 28 edges
9. `req_mode01()` - 22 edges
10. `nvlog_service()` - 22 edges

## Surprising Connections (you probably didn't know these)
- `5. Known gaps for a "complete" in-car build` --references--> `backlight_set()`  [INFERRED]
  docs/vehicle-install.md → Core/Src/app_main.c
- `Backlight control module` --references--> `backlight_set()`  [INFERRED]
  docs/wiring.md → Core/Src/app_main.c
- `1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS` --references--> `AppMain_Init()`  [INFERRED]
  docs/bench-first-flash.md → Core/Src/app_main.c
- `Подсветка: драйвер затвора` --references--> `AppMain_Init()`  [INFERRED]
  docs/future/build-hardware.md → Core/Src/app_main.c
- `Низкий приоритет и гигиена` --references--> `fmt()`  [INFERRED]
  docs/REVIEW.md → Core/Src/cluster_ui.c

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Three-Page Cluster Navigation (Drive/DPF/Diag)** — design_diesel_cluster_v2_3_drive_page, design_diesel_cluster_v2_3_dpf_page, design_diesel_cluster_v2_3_diag_page, design_diesel_cluster_v2_3_tab_switcher [EXTRACTED 1.00]
- **Glanceable Diesel-Health Signature Elements** — design_diesel_cluster_v2_3_systems_rail, design_diesel_cluster_v2_3_calibration_eyebrow, design_diesel_cluster_v2_3_state_colour_encoding, design_diesel_cluster_v2_3_ili9488_constraints [INFERRED 0.75]

## Communities (78 total, 5 thin omitted)

### Community 0 - "obd_rx_poll"
Cohesion: 0.08
Nodes (39): obd_rx_poll(), metric_key_t, obd_data_t, obd_is_fresh(), last_tx_is(), near(), sniff_saw(), t_boost_needs_baro_and_follows_map() (+31 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "string"
Cohesion: 0.13
Nodes (37): ms_txt(), refresh_stats(), speed_cal_band(), speed_cal_factor(), speed_cal_gps(), speed_cal_obd(), speed_cal_reset(), steady() (+29 more)

### Community 3 - "test_can_sniff.c"
Cohesion: 0.12
Nodes (20): can_sniff_filter_error(), stdio, FDCAN_FilterTypeDef, FDCAN_HandleTypeDef, HAL_StatusTypeDef, feed(), HAL_FDCAN_ConfigFilter(), main() (+12 more)

### Community 4 - "lv_port_disp.c"
Cohesion: 0.17
Nodes (28): lv_area_t, lv_display_t, SPI_HandleTypeDef, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb() (+20 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.20
Nodes (13): assert_text(), fault_init(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_selftest(), fault_wdg_kick(), fault_wdg_run_mode() (+5 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.14
Nodes (22): arrived_in_time(), can_send(), dpf_refused(), mil_attempt_failed(), miss_counts(), mode01_covers(), mode01_len(), oilp_refused() (+14 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "test_alert.c"
Cohesion: 0.18
Nodes (25): alert, alert_cfg_t, alert_t, alert_reset(), alert_step(), alerts_engine_running(), alerts_state(), metric_key_t (+17 more)

### Community 11 - "test_lv_port_disp.c"
Cohesion: 0.11
Nodes (35): lv_port_disp_init(), lv_port_disp_service(), lv_display_flush_cb_t, IRQn_Type, lv_display_t, lv_obj_t, dma_done(), flush() (+27 more)

### Community 12 - "stdint"
Cohesion: 0.11
Nodes (7): metric_key_t, metric_state_t, metric_state(), fw_version, stdbool, stddef, stdint

### Community 13 - "cluster_ui_refresh"
Cohesion: 0.14
Nodes (15): cluster_ui_refresh(), hex8(), obd_dpf_t, obd_health_t, obd_oilp_probe_t, obd_pid_support_t, obd_can_health(), obd_dpf() (+7 more)

### Community 14 - "obd_poll_tick"
Cohesion: 0.11
Nodes (26): obd_poll_tick(), pidsup_next(), req_mode01(), req_mode22(), req_pidsup(), cap_scenario(), t_c3_01_cf_after_cap_rejected(), t_c8_medium_rotation_even() (+18 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 17 - "Src/main.c"
Cohesion: 0.24
Nodes (5): app_main, TIM_HandleTypeDef, HAL_TIM_PeriodElapsedCallback(), fault, main

### Community 18 - "irq_mode"
Cohesion: 0.16
Nodes (22): FDCAN1_IT0_IRQHandler(), c9_busoffs(), c9_run(), c9_until_busoff(), irq_mode(), t_c8_boost_and_gear_stamps(), t_c9_busoff_backs_off_then_listens(), t_c9_busoff_once_restarts_at_once() (+14 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "test_nvlog.c"
Cohesion: 0.10
Nodes (55): nv_data_t, nv_info_t, enter_next_sector(), erased(), error(), get32(), next_sector(), nv_crc32() (+47 more)

### Community 23 - "assembly_steps.py"
Cohesion: 0.09
Nodes (18): cap(), diode(), esc(), module(), Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per…, Resistor between two points on one axis; label beside the body., Ceramic capacitor (no polarity), vertical., Vertical diode body with its band (cathode). (+10 more)

### Community 24 - "tim.c"
Cohesion: 0.43
Nodes (6): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init(), tim

### Community 25 - "test_gps.c"
Cohesion: 0.06
Nodes (60): enter(), gps_init(), gps_poll(), gps_hw_getc(), gps_hw_init(), gps_hw_line_ms(), gps_hw_now_ms(), gps_hw_overruns() (+52 more)

### Community 27 - "HAL_GetTick"
Cohesion: 0.15
Nodes (20): apply_filter(), sniff_cand_t, sniff_mover_t, can_sniff_candidates(), can_sniff_feed(), can_sniff_fps(), can_sniff_id_count(), can_sniff_is_active() (+12 more)

### Community 28 - "test_input.c"
Cohesion: 0.22
Nodes (20): btn_out_t, btn_t, btn_step(), daynight_init(), daynight_step(), daynight_t, exti_press(), hold_level() (+12 more)

### Community 29 - "fault_record"
Cohesion: 0.29
Nodes (7): fault_assert_failed(), fault_record(), vApplicationStackOverflowHook(), MX_FREERTOS_Init(), HardFault_Handler(), fault_code_t, TaskHandle_t

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "cluster_ui.c"
Cohesion: 0.05
Nodes (122): build_diag(), build_drive(), build_perf(), build_sniff(), build_stats(), build_strip(), build_trip(), lv_font_t (+114 more)

### Community 32 - "obd_init"
Cohesion: 0.13
Nodes (25): FDCAN_HandleTypeDef, HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), rx_pump(), Таблица пунктов, FDCAN_ProtocolStatusTypeDef, FDCAN_RxHeaderTypeDef (+17 more)

### Community 33 - "CLAUDE.md"
Cohesion: 0.29
Nodes (5): Build / flash / debug, Git workflow: commit and push automatically, graphify, Use graphify before and after non-trivial changes, What this is

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "build.py"
Cohesion: 0.15
Nodes (22): re, check_defines(), compile_one(), find_programmer(), find_toolchain(), flags_stamp(), flash(), known_switches() (+14 more)

### Community 36 - "Экран: качество картинки и скорость"
Cohesion: 0.10
Nodes (20): 0. Обновление 2026-10-22: «двоится», два глаза видят разные цвета, 1. Что сейчас и где теряется, 2. Время передачи кадра, 3. Этап 0: без покупок (сделать в любом случае), 4. Варианты железа, 5. Рекомендация, 6. Что купить, 6a. Что из этого есть на LCSC (проверено 2026-10-09) (+12 more)

### Community 37 - "Hardware reference — WeAct MiniSTM32H743"
Cohesion: 0.13
Nodes (15): heartbeat_led_init(), heartbeat_led_tick(), Backlight control module, Boot / programming, CAN transceiver module, Core board, External modules (as purchased), Hardware reference — WeAct MiniSTM32H743 (+7 more)

### Community 39 - "Плата-носитель 7×9 см: раскладка и провода"
Cohesion: 0.15
Nodes (13): 1. Распиновка гребёнок WeAct, 2. Гнёзда под WeAct: хватит двух 1×40, 3. Клеммники 3.81 мм на сетке 2.54: гнёзда на проводах, 4. Что где стоит, 5. Провода, 6. Проверки до установки WeAct, 7. Высота в коробке 140×82×38, Вилка и провода (+5 more)

### Community 40 - "In-vehicle install — from bench demo to a live car"
Cohesion: 0.22
Nodes (9): 1. Powering the board in the car, 3. What the firmware sends, 4. Bring-up sequence, 5. Known gaps for a "complete" in-car build, Appendix: full connection & power map, In-vehicle install — from bench demo to a live car, Power distribution, Signal wiring (all MCU I/O is 3.3 V logic — do not exceed ~3.6 V on any pin) (+1 more)

### Community 41 - "test_odo.c"
Cohesion: 0.17
Nodes (31): add(), odo_t, from(), odo_gps(), odo_init(), odo_moving(), odo_obd(), odo_reset_a() (+23 more)

### Community 42 - "GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98"
Cohesion: 0.25
Nodes (8): Added, awaiting on-car confirmation, Confirmed on this vehicle, DPF probe (2026-10-23), GM-enhanced DIDs (mode 22) for the Colorado 2.8 / E98, Oil pressure — mode 22 confirmed absent on this E98, Oil pressure via passive CAN broadcast (superseded), Sources, Standard diesel PIDs: what the support maps say (not polled)

### Community 43 - "wiring_overview.py"
Cohesion: 0.27
Nodes (13): a(), box(), conn(), dot(), esc(), net(), Generate docs/future/wiring-overview.svg: what connects where, car -> box ->…, Net tag: name drawn in a small flag, wire end at (x, y). (+5 more)

### Community 44 - "HAL_FDCAN_Init"
Cohesion: 0.33
Nodes (6): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), fdcan, HAL_FDCAN_Init()

### Community 45 - "ui_render.py"
Cohesion: 0.13
Nodes (22): argparse, concurrent_futures, hashlib, importlib_util, os, pathlib, shutil, struct (+14 more)

### Community 46 - "Первая прошивка stage-c: чек-лист"
Cohesion: 0.13
Nodes (15): 0. Какой образ шить, 1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS, 2. Образ DEMO: цвета и тревоги без машины, 3. Проверка сбросов (FAULT_TEST), 4. Когда придёт GPS (ATGM336H), 5. Когда будет плата-носитель, 6.1 Ключ на ON, двигатель заглушён — фото, 6.2 Давление масла — по желанию, только стоя в P (+7 more)

### Community 47 - "obd_on_update"
Cohesion: 0.44
Nodes (11): obd_on_stamp(), obd_on_update(), decode_mode01(), decode_mode22(), dispatch(), dpf_value(), oilp_value(), pidsup_store() (+3 more)

### Community 48 - "2. Connecting to the CAN bus (OBD-II port)"
Cohesion: 0.67
Nodes (3): 2. Connecting to the CAN bus (OBD-II port), ⚠️ Bus termination — the #1 thing to get right, Sharing the OBD port with another device

### Community 49 - "Architecture"
Cohesion: 0.31
Nodes (10): Architecture, FDCAN_HandleTypeDef, cluster_app_init(), feed_gps(), gps_gga_fresh(), gps_state_t, gps_good(), trip_app_gps() (+2 more)

### Community 50 - "txn_failed"
Cohesion: 0.17
Nodes (13): dpf_failed(), dpf_index(), oilp_failed(), req_dpf(), txn_failed(), dpf_requests(), t_c7_tx_lost_counted(), t_c8_dpf_one_refusal_then_answer() (+5 more)

### Community 51 - "AppMain_Run"
Cohesion: 0.20
Nodes (14): AppMain_Init(), AppMain_Run(), backlight_set(), bl_active(), bl_edit(), btn_snapshot(), buttons_init(), dn_low_take() (+6 more)

### Community 52 - "carrier_template.py"
Cohesion: 0.15
Nodes (9): pymupdf, board(), esc(), key_mark(), prow(), Generate docs/future/carrier-template.pdf: the 7x9 carrier at 1:1 on A4, to…, Key position: the socket pin is pulled out, nothing goes into this hole., Holes, labels, sockets, blocks and zones; mirror = view from the solder side. (+1 more)

### Community 54 - "Dash — обзор проекта для ревью"
Cohesion: 0.17
Nodes (12): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 4. Сборка, прошивка, инструменты, 5. Находки ревью, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору, 9. В каком порядке читать (+4 more)

### Community 55 - "carrier_board.py"
Cohesion: 0.44
Nodes (9): a(), block(), hx(), hy(), key_mark(), part(), pin_xy(), prow() (+1 more)

### Community 56 - "oilp_standstill"
Cohesion: 0.40
Nodes (6): metric_key_t, obd_data_t, obd_is_fresh(), obd_mil_fresh(), obd_sel_fresh(), oilp_standstill()

### Community 57 - "Сборка платы-носителя по шагам: что куда паять"
Cohesion: 0.18
Nodes (11): После всех шагов, Сборка платы-носителя по шагам: что куда паять, Шаг 0. Гнёзда клеммников X1–X5 и ключи, Шаг 1. Питание, Шаг 2. Шины, Шаг 3. Экран (X5), Шаг 4. Подсветка (2N7000 + Si4599), Шаг 5. CAN (X2) (+3 more)

### Community 58 - "harness_tree.py"
Cohesion: 0.44
Nodes (8): a(), box(), cable(), esc(), plug(), Generate docs/future/harness.svg: the harness as a tree, box -> car / display /…, A bundle: thin coloured lines side by side, each segment offset across its own…, text()

### Community 59 - "test_perf.c"
Cohesion: 0.10
Nodes (42): perf_t, cross_t(), no_gps(), perf_clear(), perf_elapsed(), perf_gps(), perf_init(), perf_tick() (+34 more)

### Community 60 - "req_oilp"
Cohesion: 0.14
Nodes (18): req_oilp(), FDCAN_TxHeaderTypeDef, ecm_talks(), HAL_FDCAN_AddMessageToTxFifoQ(), park(), t_c8_cap_not_counted_after_sniff(), t_c8_lost_tx_is_not_a_miss(), t_c8_silent_ecm_is_not_a_miss() (+10 more)

### Community 61 - "3. Архитектура"
Cohesion: 0.33
Nodes (6): 3. Архитектура, Ключевые соглашения (их легко сломать), Модули, Планировщик запросов (`obd_poll_tick`), Поток управления, Фильтры FDCAN

### Community 62 - "Жгут: провода от коробки к машине, экрану, кнопкам и GPS"
Cohesion: 0.25
Nodes (8): X1 — питание, X2 — CAN, X3 — GPS, X4 — кнопки и габариты, X5 — экран, Жгут: провода от коробки к машине, экрану, кнопкам и GPS, Общие правила, Перед первым включением в машине

### Community 63 - "Error_Handler"
Cohesion: 0.24
Nodes (9): HAL_StatusTypeDef, cfg_ok(), Error_Handler(), SystemClock_Config(), SPI_HandleTypeDef, HAL_SPI_MspDeInit(), HAL_SPI_MspInit(), MX_SPI2_Init() (+1 more)

### Community 64 - "Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру"
Cohesion: 0.22
Nodes (15): Hardware notes worth knowing before touching related code, lv_color_t, ui_bg_color(), ui_border_color(), ui_text(), ui_text_color(), DMA1_Stream0_IRQHandler(), spi2_dma_init() (+7 more)

### Community 65 - "Oil pressure: where it comes from, and how to check it"
Cohesion: 0.25
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 66 - "main"
Cohesion: 0.40
Nodes (4): MX_GPIO_Init(), main(), MPU_Config(), gpio

### Community 68 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.29
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 69 - "HAL_StatusTypeDef"
Cohesion: 0.36
Nodes (8): SPI2_IRQHandler(), HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_Init(), HAL_SPI_IRQHandler(), HAL_SPI_Transmit(), HAL_SPI_Transmit_DMA()

### Community 70 - "ui_render/main.c"
Cohesion: 0.15
Nodes (10): lv_tick_get_cb_t, lvgl, odo, perf, lv_init(), lv_tick_set_cb(), dump(), main() (+2 more)

### Community 72 - "stubs.c"
Cohesion: 0.05
Nodes (31): nvlog, stdlib, gps_state_t, metric_key_t, nv_info_t, obd_data_t, obd_dpf_t, obd_health_t (+23 more)

### Community 73 - "cluster_policy.h"
Cohesion: 0.30
Nodes (14): metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh(), policy_fresh(), policy_shown(), policy_sniff_allowed(), policy_summary() (+6 more)

### Community 74 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

### Community 75 - "math"
Cohesion: 0.21
Nodes (11): btn_hold(), odo_t, perf_t, trip_sel_t, trip_app_odo(), trip_app_perf(), trip_app_perf_clear(), trip_app_reset_selected() (+3 more)

### Community 76 - "lv_area_get_width"
Cohesion: 0.33
Nodes (6): lv_area_t, lv_area_get_height(), lv_area_get_width(), lv_area_t, lv_display_t, flush_cb()

### Community 78 - "app_main.c"
Cohesion: 0.18
Nodes (12): can_sniff, cluster_app, cluster_ui, app_loop_max_ms(), btn_down(), HAL_GPIO_EXTI_Callback(), fdcan_obd, input (+4 more)

### Community 79 - "btn_tap"
Cohesion: 0.40
Nodes (6): btn_tap(), cluster_ui_get_page(), cluster_ui_next_page(), cluster_ui_prev_page(), sniff_allowed(), trip_app_select_next()

### Community 83 - "test_fdcan_obd.c"
Cohesion: 0.09
Nodes (29): IRQn_Type, metric_key_t, egt_reply(), egt_request(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), main() (+21 more)

## Knowledge Gaps
- **189 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Use graphify before and after non-trivial changes`, `graphify`, `Фото 7 (625) — двигатель работает, RPM=1056` (+184 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 442 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Architecture` connect `Architecture` to `obd_rx_poll`, `fault.c`, `fdcan_obd.c`, `test_alert.c`, `test_lv_port_disp.c`, `stdint`, `cluster_ui_refresh`, `obd_poll_tick`, `irq_mode`, `test_nvlog.c`, `test_gps.c`, `HAL_GetTick`, `test_input.c`, `cluster_ui.c`, `obd_init`, `CLAUDE.md`, `obd_on_update`, `txn_failed`, `AppMain_Run`, `test_perf.c`, `Error_Handler`, `cluster_policy.h`, `btn_tap`?**
  _High betweenness centrality (0.098) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `AppMain_Run` to `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру`, `Чистовая сборка: из макетки в устройство`, `Hardware reference — WeAct MiniSTM32H743`, `fault.c`, `test_lv_port_disp.c`, `app_main.c`, `Первая прошивка stage-c: чек-лист`, `Architecture`, `test_gps.c`, `HAL_GetTick`, `test_input.c`, `cluster_ui.c`?**
  _High betweenness centrality (0.076) - this node is a cross-community bridge._
- **Why does `cluster_ui_refresh()` connect `cluster_ui_refresh` to `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру`, `string`, `Экран: качество картинки и скорость`, `ui_render/main.c`, `cluster_policy.h`, `test_alert.c`, `app_main.c`, `btn_tap`, `Architecture`, `Dash — обзор проекта для ревью`, `oilp_standstill`, `HAL_GetTick`, `3. Архитектура`, `cluster_ui.c`?**
  _High betweenness centrality (0.075) - this node is a cross-community bridge._
- **Are the 90 inferred relationships involving `obd_rx_poll()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_rx_poll()` has 90 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `cluster_ui_refresh()` (e.g. with `Architecture` and `Hardware notes worth knowing before touching related code`) actually correct?**
  _`cluster_ui_refresh()` has 39 INFERRED edges - model-reasoned connections that need verification._
- **Are the 40 inferred relationships involving `obd_poll_tick()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_poll_tick()` has 40 INFERRED edges - model-reasoned connections that need verification._
- **Are the 42 inferred relationships involving `Architecture` (e.g. with `metric_state()` and `policy_complete()`) actually correct?**
  _`Architecture` has 42 INFERRED edges - model-reasoned connections that need verification._