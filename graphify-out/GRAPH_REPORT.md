# Graph Report - Dash  (2026-10-26)

## Corpus Check
- 116 files · ~893,433 words
- Verdict: corpus is large enough that graph structure adds value.
- Unclassified: 9 file(s) not represented in the graph (top: (none) 5, .ld 2, .ioc 1)

## Summary
- 1521 nodes · 3543 edges · 80 communities (75 shown, 5 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 964 edges (avg confidence: 0.86)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `5aa015c7`
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
- cluster_ui.c
- obd_poll_tick
- Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото
- stm32h7xx_it.c
- Src/main.c
- FDCAN1_IT0_IRQHandler
- stm32h7xx_hal_conf.h
- Свой одометр по GPS и расход топлива
- test_nvlog.c
- assembly_steps.py
- tim.c
- test_gps.c
- cluster_ui_refresh
- test_input.c
- st7735_status.c
- Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)
- test_st7735_status.c
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
- st7735_status_set
- ui_render.py
- Первая прошивка stage-c: чек-лист
- obd_on_update
- reply_is_ours
- Architecture
- req_mode22
- AppMain_Run
- carrier_template.py
- main.h
- Dash — обзор проекта для ревью
- carrier_board.py
- Глубокий аудит Stage C2 (дополнение, 2026-10-10) — статус
- Сборка платы-носителя по шагам: что куда паять
- harness_tree.py
- math
- req_oilp
- HAL_StatusTypeDef
- Жгут: провода от коробки к машине, экрану, кнопкам и GPS
- Error_Handler
- set_metric
- Oil pressure: where it comes from, and how to check it
- HAL_GPIO_Init
- Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру
- SNIFF capture — selector / PRNDL search
- HAL_StatusTypeDef
- lv_port_disp_init
- obd_data_t
- stubs.c
- cluster_policy.h
- HAL_GPIO_Init
- trip_app.c
- lv_area_get_width
- app_main.c
- cluster_ui_set_page
- test_fdcan_obd.c

## God Nodes (most connected - your core abstractions)
1. `obd_rx_poll()` - 102 edges
2. `cluster_ui_refresh()` - 56 edges
3. `obd_poll_tick()` - 46 edges
4. `Architecture` - 43 edges
5. `st7735_status_init()` - 36 edges
6. `HAL_GetTick()` - 31 edges
7. `near()` - 29 edges
8. `AppMain_Run()` - 28 edges
9. `nvlog_service()` - 22 edges
10. `AppMain_Init()` - 21 edges

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

## Communities (80 total, 5 thin omitted)

### Community 0 - "obd_rx_poll"
Cohesion: 0.08
Nodes (40): obd_rx_poll(), obd_data_t, obd_is_fresh(), egt_reply(), egt_request(), near(), t_boost_needs_baro_and_follows_map(), t_c7_egt_mask() (+32 more)

### Community 1 - "Чистовая сборка: из макетки в устройство"
Cohesion: 0.06
Nodes (33): 1. Что не так с макетом, 2. Выбранная схема (вариант A) и что потом, 3. Схема узлов (вариант A), 4. Корпус и установка, 5. Порядок сборки и проверки, 6. Стоимость варианта A, 6a. Если бюджет позволяет (было в первой версии, ~$100–150), 6b. Заказ на LCSC (проверено по API LCSC 2026-10-11) (+25 more)

### Community 2 - "string"
Cohesion: 0.13
Nodes (37): ms_txt(), refresh_stats(), speed_cal_band(), speed_cal_factor(), speed_cal_gps(), speed_cal_obd(), speed_cal_reset(), steady() (+29 more)

### Community 3 - "test_can_sniff.c"
Cohesion: 0.14
Nodes (18): can_sniff_filter_error(), FDCAN_FilterTypeDef, FDCAN_HandleTypeDef, HAL_StatusTypeDef, feed(), HAL_FDCAN_ConfigFilter(), main(), reset() (+10 more)

### Community 4 - "lv_port_disp.c"
Cohesion: 0.21
Nodes (23): fault_selftest(), lv_area_t, lv_display_t, cs_high(), cs_low(), dc_cmd(), dc_data(), disp_flush_cb() (+15 more)

### Community 5 - "Colorado 2.8 Diesel Cluster v2 Mockup"
Cohesion: 0.22
Nodes (13): Calibration Eyebrow, Colorado 2.8 Diesel Cluster v2 Mockup, Holden Colorado RG 2.8 E98 ECM, DIAG Page, DPF Page, DPF Soot / Regeneration Monitoring, DRIVE Page, ECU Instrument Direction (+5 more)

### Community 6 - "fault.c"
Cohesion: 0.14
Nodes (19): assert_text(), fault_assert_failed(), fault_init(), fault_record(), fault_reset_abnormal(), fault_reset_streak(), fault_reset_text(), fault_wdg_kick() (+11 more)

### Community 7 - "fdcan_obd.c"
Cohesion: 0.13
Nodes (24): obd_dpf_t, obd_health_t, obd_pid_support_t, dpf_failed(), dpf_index(), dpf_refused(), mil_attempt_failed(), miss_counts() (+16 more)

### Community 8 - "stm32h7xx_hal_timebase_tim.c"
Cohesion: 0.33
Nodes (3): HAL_StatusTypeDef, HAL_InitTick(), stm32h7xx_hal_tim

### Community 9 - "SNIFF page (STATE + ANALOG)"
Cohesion: 0.10
Nodes (19): DIAG page, DPF page, DRIVE page, SNIFF page (STATE + ANALOG), Сессия 2026-07-23, утро (~10:10) — расшифровка фото, Фото 10 (628) — двигатель не запущен, Фото 11 (629), Фото 12 (630) — двигатель заглушен, WARN (+11 more)

### Community 10 - "test_alert.c"
Cohesion: 0.17
Nodes (26): alert, alert_cfg_t, alert_t, alert_reset(), alert_step(), alerts_engine_running(), alerts_state(), metric_key_t (+18 more)

### Community 11 - "test_lv_port_disp.c"
Cohesion: 0.19
Nodes (19): lv_port_disp_service(), IRQn_Type, dma_done(), flush(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), t_boot_errors_then_good_dma(), t_dma_refused_fallback_fails() (+11 more)

### Community 12 - "stdint"
Cohesion: 0.11
Nodes (7): metric_key_t, metric_state_t, metric_state(), fw_version, stdbool, stddef, stdint

### Community 13 - "cluster_ui.c"
Cohesion: 0.38
Nodes (26): build_diag(), build_drive(), build_perf(), build_sniff(), build_stats(), build_strip(), build_trip(), lv_font_t (+18 more)

### Community 14 - "obd_poll_tick"
Cohesion: 0.08
Nodes (31): obd_poll_tick(), req_mode01(), req_pidsup(), 3. Архитектура, Ключевые соглашения (их легко сломать), Модули, Планировщик запросов (`obd_poll_tick`), Поток управления (+23 more)

### Community 15 - "Сессия 2026-07-24, утро (~10:19–10:20) — расшифровка фото"
Cohesion: 0.11
Nodes (17): 1. OIL P нестабильна — приоритет высокий, 2. DPF данные нестабильны, 3. DPF page WARN при dP=NaN, 4. BATT 11.9V — WARN alert, DIAG page, DPF page, DRIVE page, SNIFF page (+9 more)

### Community 17 - "Src/main.c"
Cohesion: 0.24
Nodes (5): app_main, TIM_HandleTypeDef, HAL_TIM_PeriodElapsedCallback(), fault, main

### Community 18 - "FDCAN1_IT0_IRQHandler"
Cohesion: 0.27
Nodes (13): FDCAN1_IT0_IRQHandler(), irq_mode(), t_c8_boost_and_gear_stamps(), t_d05_arrival_check_across_wrap(), t_d05_late_arrival_is_not_an_answer(), t_d06_sniff_gets_rx_time(), t_delayed_frame_across_wrap(), t_delayed_frame_keeps_rx_time() (+5 more)

### Community 19 - "stm32h7xx_hal_conf.h"
Cohesion: 0.03
Nodes (61): stm32h7xx_hal_adc, stm32h7xx_hal_cec, stm32h7xx_hal_comp, stm32h7xx_hal_cordic, stm32h7xx_hal_cortex, stm32h7xx_hal_crc, stm32h7xx_hal_cryp, stm32h7xx_hal_dac (+53 more)

### Community 21 - "Свой одометр по GPS и расход топлива"
Cohesion: 0.18
Nodes (11): 1. Что показываем, 2. Пробег: откуда брать расстояние, 3. Расход топлива: откуда брать, 4. Хранение, 5. Экран и управление, 6. Прошивка: из чего состоит, 7. Что сделать первым, Источники (+3 more)

### Community 22 - "test_nvlog.c"
Cohesion: 0.10
Nodes (56): nv_data_t, nv_info_t, enter_next_sector(), erased(), error(), get32(), next_sector(), nv_crc32() (+48 more)

### Community 23 - "assembly_steps.py"
Cohesion: 0.09
Nodes (18): cap(), diode(), esc(), module(), Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per…, Resistor between two points on one axis; label beside the body., Ceramic capacitor (no polarity), vertical., Vertical diode body with its band (cathode). (+10 more)

### Community 24 - "tim.c"
Cohesion: 0.43
Nodes (6): TIM_HandleTypeDef, HAL_TIM_MspPostInit(), HAL_TIM_PWM_MspDeInit(), HAL_TIM_PWM_MspInit(), MX_TIM1_Init(), tim

### Community 25 - "test_gps.c"
Cohesion: 0.06
Nodes (60): enter(), gps_init(), gps_poll(), gps_hw_getc(), gps_hw_init(), gps_hw_line_ms(), gps_hw_now_ms(), gps_hw_overruns() (+52 more)

### Community 27 - "cluster_ui_refresh"
Cohesion: 0.14
Nodes (24): apply_filter(), sniff_cand_t, sniff_mover_t, can_sniff_candidates(), can_sniff_feed(), can_sniff_fps(), can_sniff_id_count(), can_sniff_is_active() (+16 more)

### Community 28 - "test_input.c"
Cohesion: 0.22
Nodes (20): btn_out_t, btn_t, btn_step(), daynight_init(), daynight_step(), daynight_t, exti_press(), hold_level() (+12 more)

### Community 29 - "st7735_status.c"
Cohesion: 0.31
Nodes (13): lv_display_t, st7735_status_spi_errors(), st_broken(), st_cs_high(), st_cs_low(), st_dc_cmd(), st_dc_data(), st_hw_init() (+5 more)

### Community 30 - "Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке)"
Cohesion: 0.20
Nodes (9): Анализ: что ответили новые enhanced-DID, Выводы / что дальше, КРИТИЧНО: скорость/обороты/rail/batt/load не читались, Прогон 2026-07-21 (после ~5 мин поездки, снимок на парковке), Расшифровка, Фото 1–4 — SNIFF (широковещательные кадры, мотор заглушён), Фото 5 — DIAG, Фото 6 — DPF (+1 more)

### Community 31 - "test_st7735_status.c"
Cohesion: 0.11
Nodes (39): st7735_status_init(), st7735_status_recoveries(), st7735_status_service(), lv_lcd_flag_t, lv_st7735_send_cmd_cb_t, lv_st7735_send_color_cb_t, stdarg, lv_color_t (+31 more)

### Community 32 - "obd_init"
Cohesion: 0.11
Nodes (30): FDCAN_HandleTypeDef, can_send(), HAL_FDCAN_RxFifo0Callback(), obd_check_health(), obd_init(), request(), rx_pump(), send_flow_control() (+22 more)

### Community 33 - "CLAUDE.md"
Cohesion: 0.29
Nodes (5): Build / flash / debug, Git workflow: commit and push automatically, graphify, Use graphify before and after non-trivial changes, What this is

### Community 34 - "Findings"
Cohesion: 0.20
Nodes (9): 1. OIL P (oil pressure) — candidate 0x1BA[3] is NOT oil pressure — REJECT, 2. SPEED — ~10 s lag + ~12 % high, 3. OIL temp — plausible magnitude, trend to verify, 4. Reads OK (no action), DIAG page (photos 17–18), DRIVE page readings (photos 19–26), Findings, Road-test session — 2026-08-23 (26 photos) (+1 more)

### Community 35 - "build.py"
Cohesion: 0.16
Nodes (20): re, compile_one(), find_programmer(), find_toolchain(), flags_stamp(), flash(), known_switches(), lint() (+12 more)

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

### Community 44 - "st7735_status_set"
Cohesion: 0.26
Nodes (13): st7735_status_backlight(), st7735_status_key_dbg(), st7735_status_note(), st7735_status_set(), st_link_t, lv_label_set_text_fmt(), lv_lock(), lv_obj_add_flag() (+5 more)

### Community 45 - "ui_render.py"
Cohesion: 0.13
Nodes (22): argparse, concurrent_futures, hashlib, importlib_util, os, pathlib, shutil, struct (+14 more)

### Community 46 - "Первая прошивка stage-c: чек-лист"
Cohesion: 0.13
Nodes (15): 0. Какой образ шить, 1. Стол сейчас: WeAct + 4" на макетке, KEY, без машины и GPS, 2. Образ DEMO: цвета и тревоги без машины, 3. Проверка сбросов (FAULT_TEST), 4. Когда придёт GPS (ATGM336H), 5. Когда будет плата-носитель, 6.1 Ключ на ON, двигатель заглушён — фото, 6.2 Давление масла — по желанию, только стоя в P (+7 more)

### Community 47 - "obd_on_update"
Cohesion: 0.44
Nodes (11): obd_on_stamp(), obd_on_update(), decode_mode01(), decode_mode22(), dispatch(), dpf_value(), oilp_value(), pidsup_store() (+3 more)

### Community 48 - "reply_is_ours"
Cohesion: 0.18
Nodes (11): mode01_covers(), mode01_len(), pid_requested(), reply_is_ours(), 5. Находки ревью, Высокий приоритет, Низкий приоритет и гигиена, Средний приоритет (+3 more)

### Community 49 - "Architecture"
Cohesion: 0.27
Nodes (10): Architecture, btn_snapshot(), FDCAN_HandleTypeDef, cluster_app_init(), feed_gps(), cat(), gps_gga_fresh(), trip_app_service() (+2 more)

### Community 50 - "req_mode22"
Cohesion: 0.22
Nodes (9): req_dpf(), req_mode22(), t_c8_dpf_one_refusal_then_answer(), t_d03_nrc78_total_cap(), t_d03_nrc78_within_cap_completes(), t_dpf_refusal_stops_asking(), t_dpf_silence_gives_up(), t_nrc78_keeps_slot() (+1 more)

### Community 51 - "AppMain_Run"
Cohesion: 0.27
Nodes (11): AppMain_Init(), AppMain_Run(), backlight_set(), bl_active(), bl_edit(), buttons_init(), dn_low_take(), StartDefaultTask() (+3 more)

### Community 52 - "carrier_template.py"
Cohesion: 0.15
Nodes (9): pymupdf, board(), esc(), key_mark(), prow(), Generate docs/future/carrier-template.pdf: the 7x9 carrier at 1:1 on A4, to…, Key position: the socket pin is pulled out, nothing goes into this hole., Holes, labels, sockets, blocks and zones; mirror = view from the solder side. (+1 more)

### Community 54 - "Dash — обзор проекта для ревью"
Cohesion: 0.22
Nodes (9): 1. Что это, 2. Что работает в машине (по заметкам с выездов), 4. Сборка, прошивка, инструменты, 6. Что сделано хорошо, 7. Устаревшая документация (расходится с кодом), 8. Вопросы автору, 9. В каком порядке читать, Dash — обзор проекта для ревью (+1 more)

### Community 55 - "carrier_board.py"
Cohesion: 0.44
Nodes (9): a(), block(), hx(), hy(), key_mark(), part(), pin_xy(), prow() (+1 more)

### Community 56 - "Глубокий аудит Stage C2 (дополнение, 2026-10-10) — статус"
Cohesion: 0.29
Nodes (8): arrived_in_time(), metric_key_t, obd_data_t, obd_is_fresh(), obd_mil_fresh(), obd_sel_fresh(), oilp_standstill(), Глубокий аудит Stage C2 (дополнение, 2026-10-10) — статус

### Community 57 - "Сборка платы-носителя по шагам: что куда паять"
Cohesion: 0.18
Nodes (11): После всех шагов, Сборка платы-носителя по шагам: что куда паять, Шаг 0. Гнёзда клеммников X1–X5 и ключи, Шаг 1. Питание, Шаг 2. Шины, Шаг 3. Экран (X5), Шаг 4. Подсветка (2N7000 + Si4599), Шаг 5. CAN (X2) (+3 more)

### Community 58 - "harness_tree.py"
Cohesion: 0.44
Nodes (8): a(), box(), cable(), esc(), plug(), Generate docs/future/harness.svg: the harness as a tree, box -> car / display /…, A bundle: thin coloured lines side by side, each segment offset across its own…, text()

### Community 59 - "math"
Cohesion: 0.10
Nodes (43): perf_t, cross_t(), no_gps(), perf_clear(), perf_elapsed(), perf_gps(), perf_init(), perf_tick() (+35 more)

### Community 60 - "req_oilp"
Cohesion: 0.13
Nodes (20): req_oilp(), ecm_talks(), last_tx_is(), park(), t_c7_tx_lost_counted(), t_c8_cap_not_counted_after_sniff(), t_c8_lost_tx_is_not_a_miss(), t_c8_silent_ecm_is_not_a_miss() (+12 more)

### Community 61 - "HAL_StatusTypeDef"
Cohesion: 0.53
Nodes (6): HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_DeInit(), HAL_SPI_Init(), HAL_SPI_Transmit()

### Community 62 - "Жгут: провода от коробки к машине, экрану, кнопкам и GPS"
Cohesion: 0.25
Nodes (8): X1 — питание, X2 — CAN, X3 — GPS, X4 — кнопки и габариты, X5 — экран, Жгут: провода от коробки к машине, экрану, кнопкам и GPS, Общие правила, Перед первым включением в машине

### Community 63 - "Error_Handler"
Cohesion: 0.12
Nodes (18): FDCAN_HandleTypeDef, HAL_FDCAN_MspDeInit(), HAL_FDCAN_MspInit(), MX_FDCAN1_Init(), HAL_StatusTypeDef, cfg_ok(), MX_GPIO_Init(), Error_Handler() (+10 more)

### Community 64 - "set_metric"
Cohesion: 0.14
Nodes (27): Hardware notes worth knowing before touching related code, lv_color_t, metric_key_t, metric_state_t, obd_data_t, fmt(), fmt_secs(), gps_brief() (+19 more)

### Community 65 - "Oil pressure: where it comes from, and how to check it"
Cohesion: 0.33
Nodes (6): Check on the car, DIAG, bottom readout (line 3), How the firmware asks (`fdcan_obd.c`, "oil pressure"), Oil pressure: where it comes from, and how to check it, The source, What the old candidates were

### Community 66 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

### Community 67 - "Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру"
Cohesion: 0.14
Nodes (17): loop_time_tick(), SPI_HandleTypeDef, DMA1_Stream0_IRQHandler(), HAL_SPI_ErrorCallback(), HAL_SPI_TxCpltCallback(), spi2_dma_init(), DMA_HandleTypeDef, Канонический аудит Stage C (2026-10-10) — статус по пунктам (+9 more)

### Community 68 - "SNIFF capture — selector / PRNDL search"
Cohesion: 0.33
Nodes (6): Analysis — distinct value set per interesting byte, Conclusion, Next, Run 1  (design/Photos/1/, frames 023–035), Run 2  (design/Photos/2/, frames 036–052), SNIFF capture — selector / PRNDL search

### Community 69 - "HAL_StatusTypeDef"
Cohesion: 0.36
Nodes (8): SPI2_IRQHandler(), HAL_StatusTypeDef, SPI_HandleTypeDef, HAL_SPI_Abort(), HAL_SPI_Init(), HAL_SPI_IRQHandler(), HAL_SPI_Transmit(), HAL_SPI_Transmit_DMA()

### Community 70 - "lv_port_disp_init"
Cohesion: 0.13
Nodes (19): lv_port_disp_init(), lv_display_flush_cb_t, lv_tick_get_cb_t, lv_display_t, lv_obj_t, lv_display_create(), lv_display_flush_ready(), lv_display_get_screen_active() (+11 more)

### Community 71 - "obd_data_t"
Cohesion: 0.40
Nodes (5): metric_key_t, obd_data_t, obd_is_fresh(), obd_mil_fresh(), obd_sel_fresh()

### Community 72 - "stubs.c"
Cohesion: 0.05
Nodes (26): nvlog, st7735_status, gps_state_t, nv_info_t, obd_dpf_t, obd_health_t, obd_oilp_probe_t, obd_pid_support_t (+18 more)

### Community 73 - "cluster_policy.h"
Cohesion: 0.33
Nodes (13): metric_key_t, obd_data_t, policy_complete(), policy_ecm_fresh(), policy_fresh(), policy_shown(), policy_sniff_allowed(), policy_summary() (+5 more)

### Community 74 - "HAL_GPIO_Init"
Cohesion: 0.40
Nodes (5): GPIO_InitTypeDef, GPIO_PinState, GPIO_TypeDef, HAL_GPIO_Init(), HAL_GPIO_WritePin()

### Community 75 - "trip_app.c"
Cohesion: 0.20
Nodes (13): btn_hold(), gps_state_t, odo_t, perf_t, trip_sel_t, gps_good(), trip_app_gps(), trip_app_odo() (+5 more)

### Community 76 - "lv_area_get_width"
Cohesion: 0.33
Nodes (6): lv_area_t, lv_area_get_height(), lv_area_get_width(), lv_area_t, lv_display_t, flush_cb()

### Community 78 - "app_main.c"
Cohesion: 0.12
Nodes (15): can_sniff, cluster_app, cluster_ui, app_loop_max_ms(), btn_down(), HAL_GPIO_EXTI_Callback(), fdcan_obd, gps (+7 more)

### Community 79 - "cluster_ui_set_page"
Cohesion: 0.25
Nodes (11): btn_tap(), cluster_ui_get_page(), cluster_ui_next_page(), cluster_ui_prev_page(), cluster_ui_set_page(), sniff_allowed(), trip_app_select_next(), dump() (+3 more)

### Community 83 - "test_fdcan_obd.c"
Cohesion: 0.10
Nodes (23): IRQn_Type, metric_key_t, dpf_requests(), HAL_NVIC_DisableIRQ(), HAL_NVIC_EnableIRQ(), HAL_NVIC_SetPriority(), main(), ps_run() (+15 more)

## Knowledge Gaps
- **189 isolated node(s):** `What this is`, `Git workflow: commit and push automatically`, `Use graphify before and after non-trivial changes`, `graphify`, `Фото 7 (625) — двигатель работает, RPM=1056` (+184 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 442 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **5 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `Architecture` connect `Architecture` to `obd_rx_poll`, `fault.c`, `fdcan_obd.c`, `test_alert.c`, `stdint`, `cluster_ui.c`, `obd_poll_tick`, `FDCAN1_IT0_IRQHandler`, `test_nvlog.c`, `test_gps.c`, `cluster_ui_refresh`, `test_input.c`, `test_st7735_status.c`, `obd_init`, `CLAUDE.md`, `obd_on_update`, `req_mode22`, `AppMain_Run`, `math`, `Error_Handler`, `lv_port_disp_init`, `cluster_policy.h`, `cluster_ui_set_page`?**
  _High betweenness centrality (0.095) - this node is a cross-community bridge._
- **Why does `AppMain_Init()` connect `AppMain_Run` to `set_metric`, `Чистовая сборка: из макетки в устройство`, `Hardware reference — WeAct MiniSTM32H743`, `fault.c`, `lv_port_disp_init`, `st7735_status_set`, `app_main.c`, `Первая прошивка stage-c: чек-лист`, `Architecture`, `test_gps.c`, `cluster_ui_refresh`, `test_input.c`, `test_st7735_status.c`?**
  _High betweenness centrality (0.086) - this node is a cross-community bridge._
- **Why does `cluster_ui_refresh()` connect `cluster_ui_refresh` to `set_metric`, `string`, `Этап 0 дисплея (`b7f9470`) — что смотреть ревьюеру`, `lv_port_disp.c`, `Экран: качество картинки и скорость`, `fdcan_obd.c`, `cluster_policy.h`, `test_alert.c`, `st7735_status_set`, `cluster_ui.c`, `app_main.c`, `cluster_ui_set_page`, `reply_is_ours`, `Architecture`, `obd_poll_tick`, `Dash — обзор проекта для ревью`, `Глубокий аудит Stage C2 (дополнение, 2026-10-10) — статус`, `st7735_status.c`?**
  _High betweenness centrality (0.067) - this node is a cross-community bridge._
- **Are the 88 inferred relationships involving `obd_rx_poll()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_rx_poll()` has 88 INFERRED edges - model-reasoned connections that need verification._
- **Are the 39 inferred relationships involving `cluster_ui_refresh()` (e.g. with `Architecture` and `Hardware notes worth knowing before touching related code`) actually correct?**
  _`cluster_ui_refresh()` has 39 INFERRED edges - model-reasoned connections that need verification._
- **Are the 38 inferred relationships involving `obd_poll_tick()` (e.g. with `Architecture` and `cluster_app_run()`) actually correct?**
  _`obd_poll_tick()` has 38 INFERRED edges - model-reasoned connections that need verification._
- **Are the 42 inferred relationships involving `Architecture` (e.g. with `metric_state()` and `policy_complete()`) actually correct?**
  _`Architecture` has 42 INFERRED edges - model-reasoned connections that need verification._