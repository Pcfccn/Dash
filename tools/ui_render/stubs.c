/* Hardware-facing functions cluster_ui.c reads, for tools/ui_render.py.
 * Every value is fresh and the bus is up; main.c fills g_obd and the GPS fix
 * with one scene. Counters are 0, so DIAG looks like a clean run. */
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
void obd_can_health(obd_health_t *h) { memset(h, 0, sizeof *h); }
void obd_oilp_probe(obd_oilp_probe_t *p) { memset(p, 0, sizeof *p); }
void obd_pid_support(obd_pid_support_t *ps) { memset(ps, 0, sizeof *ps); }
/* as this E98: no oil temp (0x5C), no EGR temp (0x6B) */
int8_t obd_pid_supported(uint8_t pid) { return (pid == 0x5C || pid == 0x6B) ? 0 : 1; }

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

bool can_sniff_set_active(bool on) { (void)on; return true; }
bool can_sniff_is_active(void) { return false; }
bool can_sniff_restore_pending(void) { return false; }
uint8_t can_sniff_candidates(sniff_cand_t *out, uint8_t max) { (void)out; (void)max; return 0; }
uint8_t can_sniff_movers(sniff_mover_t *out, uint8_t max) { (void)out; (void)max; return 0; }
uint8_t can_sniff_id_count(void) { return 0; }
uint16_t can_sniff_fps(void) { return 0; }

const char *fault_reset_text(void) { return "POWER-ON"; }
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
