/* Hardware-facing functions cluster_ui.c reads, for tools/ui_render.py.
 * Every value is fresh and the bus is up; main.c fills g_obd and the GPS fix
 * with one scene. Counters are 0, so DIAG looks like a clean run. Where the
 * real module reports a state, the stub reports one the firmware can really
 * be in (review C8: maps "not read" next to a supported 5E, POLL, READY at
 * 86 km/h were renders the panel could never show). */
#include <math.h>
#include <string.h>

#include "fdcan_obd.h"
#include "gps.h"
#include "can_sniff.h"
#include "fault.h"
#include "app_main.h"
#include "lv_port_disp.h"
#include "st7735_status.h"
#include "nvlog.h"
#include "trip_app.h"

uint32_t ui_render_ms;               /* the clock for LVGL and the UI (main.c) */
uint32_t HAL_GetTick(void) { return ui_render_ms; }

volatile obd_data_t g_obd;
/* NaN (never sent) is still shown as "--": policy_fresh() checks it first. */
bool obd_is_fresh(const volatile obd_data_t *d, metric_key_t k, uint32_t now)
{ (void)d; (void)k; (void)now; return true; }
bool obd_sel_fresh(const volatile obd_data_t *d, uint32_t now) { (void)d; (void)now; return true; }
bool obd_mil_fresh(const volatile obd_data_t *d, uint32_t now) { (void)d; (void)now; return true; }
/* RX by interrupt, as on the board (rx_irq false would draw POLL). */
void obd_can_health(obd_health_t *h) { memset(h, 0, sizeof *h); h->rx_irq = true; }
void obd_oilp_probe(obd_oilp_probe_t *p) { memset(p, 0, sizeof *p); }

/* As this E98: every mode-01 PID up to 0xA0 supported but oil temp (0x5C) and
 * EGR temp (0x6B); maps 0x00..0x80 read, 0xA0 not advertised. */
#define RENDER_MAPS 5u
int8_t obd_pid_supported(uint8_t pid)
{
    if (pid == 0u) return 1;
    if (pid > RENDER_MAPS * 0x20u) return 0;                 /* chain ended: no */
    if (pid == RENDER_MAPS * 0x20u) return 0;                /* next map: none  */
    return (pid == 0x5C || pid == 0x6B) ? 0 : 1;
}
void obd_pid_support(obd_pid_support_t *ps)
{
    memset(ps, 0, sizeof *ps);
    for (uint8_t r = 0; r < RENDER_MAPS; r++) {
        uint32_t m = 0;
        for (uint8_t b = 1; b <= 32u; b++)
            if (obd_pid_supported((uint8_t)(r * 0x20u + b)) == 1) m |= 1u << (32u - b);
        ps->mask[r] = m;
        ps->have |= (uint8_t)(1u << r);
    }
    ps->done = true;
}

/* DPF probe: soot and dp answering, regen off, since-regen answering with the
 * no-value code, as the truck did on 2026-07-21. */
void obd_dpf(obd_dpf_t *out)
{
    memset(out, 0, sizeof *out);
    static const float v[DPF_N] = { 23.0f, 1.0f, 0.0f, NAN };
    for (int i = 0; i < DPF_N; i++) {
        out->val[i] = v[i]; out->upd_ms[i] = ui_render_ms;
        out->asked[i] = 48; out->ok[i] = (i == DPF_DP) ? 46 : 48;
    }
}
bool obd_dpf_fresh(const obd_dpf_t *p, uint8_t i, uint32_t now) { (void)now; return p->ok[i] > 0u; }

gps_state_t ui_render_gps;
const gps_state_t *gps_state(void) { return &ui_render_gps; }
bool gps_fix_fresh(uint32_t now) { (void)now; return ui_render_gps.fix.rmc_fix; }

/* SNIFF: listening while its page is up, with a selector-shaped byte (0x1F5
 * byte 3: P R N D swept) and two analogue bytes in the table. */
static bool s_sniff;
bool can_sniff_set_active(bool on) { s_sniff = on; return true; }
bool can_sniff_is_active(void) { return s_sniff; }
bool can_sniff_restore_pending(void) { return false; }
uint8_t can_sniff_candidates(sniff_cand_t *out, uint8_t max)
{
    if (max < 1u) return 0;
    memset(out, 0, sizeof *out);
    out[0].id = 0x1F5; out[0].byte_idx = 3; out[0].nvals = 4;
    out[0].vals[0] = 1; out[0].vals[1] = 2; out[0].vals[2] = 3; out[0].vals[3] = 4;
    out[0].changes = 6; out[0].age_ms = 1200;
    return 1;
}
uint8_t can_sniff_movers(sniff_mover_t *out, uint8_t max)
{
    static const sniff_mover_t m[2] = {
        { .id = 0x0C9, .byte_idx = 2, .vmin = 0x10, .vmax = 0xE0, .cur = 0x7C, .changes = 211, .age_ms = 40 },
        { .id = 0x4C1, .byte_idx = 4, .vmin = 0x52, .vmax = 0x81, .cur = 0x80, .changes = 37,  .age_ms = 900 },
    };
    uint8_t n = (max < 2u) ? max : 2u;
    memcpy(out, m, n * sizeof m[0]);
    return n;
}
uint8_t can_sniff_id_count(void) { return s_sniff ? 31u : 0u; }
uint16_t can_sniff_fps(void) { return s_sniff ? 1480u : 0u; }

const char *fault_reset_text(void) { return "POWER ON"; }   /* as fault.c says it */
bool fault_reset_abnormal(void) { return false; }
uint32_t fault_reset_streak(void) { return 0; }

uint32_t app_loop_max_ms(void) { return 38; }
uint16_t lv_port_disp_spi_errors(void) { return 0; }
uint16_t lv_port_disp_recoveries(void) { return 0; }
uint16_t lv_port_disp_dma_fallbacks(void) { return 0; }
uint16_t st7735_status_spi_errors(void) { return 0; }

void nvlog_info(nv_info_t *out) { memset(out, 0, sizeof *out); out->state = NV_LOADED; out->seq = 1234; }

odo_t ui_render_odo;
perf_t ui_render_perf;
const odo_t *trip_app_odo(void) { return &ui_render_odo; }
const perf_t *trip_app_perf(void) { return &ui_render_perf; }
trip_sel_t trip_app_selected(void) { return (trip_sel_t)0; }
