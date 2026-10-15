/* Host-side tests for Core/Src/gps.c: finding the module at 115200 / 9600,
 * switching and saving its configuration, losing and regaining it, fix
 * freshness. The real gps.c and gps_nmea.c are #included; the UART
 * (gps_hw.h) is mocked together with a model of the receiver. */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "../../Core/Src/gps_nmea.c"
#include "../../Core/Src/gps.c"

static int checks, fails;
#define CHECK(cond) do { checks++; if (!(cond)) { fails++; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ---------------------------------------------------- mock UART + module */
static uint32_t now_ms;
static uint32_t uart_baud;
static int      baud_changes;

static char     written[4096];           /* everything sent to the module */
static size_t   wlen;
static int      bad_cmd_cs;              /* commands with a wrong checksum */

static uint8_t  rxq[8192];
static uint32_t rxq_at[8192];            /* receive tick of each byte (the IRQ's stamp) */
static size_t   rx_head, rx_tail;
static uint32_t line_ms;                 /* stamp of the last line end handed out */

/* the receiver */
static bool     mod_present;
static uint32_t mod_baud;                /* rate it talks at              */
static uint32_t mod_period_ms;           /* fix interval                   */
static bool     mod_saved;
static bool     mod_fix;

void     gps_hw_init(uint32_t b)     { uart_baud = b; }
void     gps_hw_set_baud(uint32_t b) { uart_baud = b; baud_changes++; }
int      gps_hw_getc(void)
{
    if (rx_head >= rx_tail) return -1;
    uint8_t b = rxq[rx_head];
    if (b == '\r' || b == '\n') line_ms = rxq_at[rx_head];
    rx_head++;
    return b;
}
uint32_t gps_hw_line_ms(void)        { return line_ms; }
uint16_t gps_hw_overruns(void)       { return 0; }
uint32_t gps_hw_now_ms(void)         { return now_ms; }

/* The module hears a command only at its own rate. */
void gps_hw_write(const char *line)
{
    size_t n = strlen(line);
    if (wlen + n < sizeof written) { memcpy(&written[wlen], line, n); wlen += n; written[wlen] = 0; }
    /* checksum check: "$body*hh\r\n" */
    const char *star = strchr(line, '*');
    char body[64] = { 0 };
    if (!star || line[0] != '$' || (size_t)(star - line - 1) >= sizeof body) { bad_cmd_cs++; return; }
    memcpy(body, line + 1, (size_t)(star - line - 1));
    unsigned cs = 0;
    if (sscanf(star + 1, "%2X", &cs) != 1 || cs != nmea_checksum(body)) bad_cmd_cs++;
    if (!mod_present || uart_baud != mod_baud) return;
    if (strcmp(body, "PCAS01,5") == 0)   mod_baud = 115200;
    if (strcmp(body, "PCAS02,100") == 0) mod_period_ms = 100;
    if (strcmp(body, "PCAS00") == 0)     mod_saved = true;
}

static void rx_push(const char *s)
{
    size_t n = strlen(s);
    if (rx_tail + n > sizeof rxq) { rx_head = rx_tail = 0; }
    memcpy(&rxq[rx_tail], s, n);
    for (size_t i = 0; i < n; i++) rxq_at[rx_tail + i] = now_ms;
    rx_tail += n;
}

/* One fix's output (RMC + GGA). At the wrong rate the UART sees garbage. */
static void mod_emit(void)
{
    if (!mod_present) return;
    if (uart_baud != mod_baud) { rx_push("\xF0\x0F\x80~~\x01"); return; }
    const char *rmc = mod_fix ? "GNRMC,021530.000,A,2728.2000,S,15301.2000,E,27.0,90.0,111026,,,A"
                              : "GNRMC,021530.000,V,,,,,,,111026,,,N";
    const char *gga = mod_fix ? "GNGGA,021530.000,2728.2000,S,15301.2000,E,1,09,1.10,25.3,M,38.0,M,,"
                              : "GNGGA,021530.000,,,,,0,03,99.99,,,,,,";
    char line[128];
    snprintf(line, sizeof line, "$%s*%02X\r\n", rmc, nmea_checksum(rmc)); rx_push(line);
    snprintf(line, sizeof line, "$%s*%02X\r\n", gga, nmea_checksum(gga)); rx_push(line);
}

/* Advance time in 10 ms steps, the module talking every mod_period_ms. */
static void run(uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10) {
        now_ms += 10;
        if (mod_period_ms && now_ms % mod_period_ms == 0) mod_emit();
        gps_poll();
    }
}

static void reset(void)
{
    now_ms = 100000; uart_baud = 0; baud_changes = 0;
    wlen = 0; written[0] = 0; bad_cmd_cs = 0; rx_head = rx_tail = 0; line_ms = 0;
    mod_present = true; mod_baud = 115200; mod_period_ms = 1000; mod_saved = false; mod_fix = true;
    gps_init();
}

static bool sent(const char *body) { return strstr(written, body) != NULL; }

/* ---------------------------------------------------------------- tests */
static void t_found_at_115200(void)
{
    run(2000);
    CHECK(gps_state()->link == GPS_LINK_RUN);
    CHECK(sent("$PCAS03,1,0,0,0,1,0,0,0,0,0,,,0,0,,,,0*") && sent("$PCAS02,100*"));
    CHECK(!sent("PCAS01") && !sent("PCAS00"));      /* no rate change, no save */
    CHECK(bad_cmd_cs == 0);
    CHECK(mod_period_ms == 100);                    /* now at 10 Hz            */
    run(2000);
    CHECK(gps_state()->rate_hz == 10);
}

static void t_found_at_9600_switched_and_saved(void)
{
    reset();
    mod_baud = 9600;                                /* factory setting         */
    run(6000);
    CHECK(gps_state()->link == GPS_LINK_RUN);
    CHECK(uart_baud == 115200 && mod_baud == 115200);
    const char *p1 = strstr(written, "PCAS01,5"), *p3 = strstr(written, "PCAS03"),
               *p2 = strstr(written, "PCAS02,100"), *p0 = strstr(written, "PCAS00");
    CHECK(p1 && p3 && p2 && p0 && p1 < p3 && p3 < p2 && p2 < p0);   /* order */
    CHECK(mod_saved && bad_cmd_cs == 0);
}

static void t_absent_retried_quietly(void)
{
    reset();
    mod_present = false;
    run(1600);
    CHECK(gps_state()->link == GPS_LINK_PROBE_SLOW && uart_baud == 9600);
    run(1600);
    CHECK(gps_state()->link == GPS_LINK_ABSENT);
    run(GPS_ABSENT_MS);
    CHECK(gps_state()->link == GPS_LINK_PROBE_FAST && uart_baud == 115200);
    CHECK(wlen == 0);                               /* never talked to nothing */
}

static void t_lost_then_regained_no_second_save(void)
{
    reset();
    mod_baud = 9600;
    run(6000);
    CHECK(gps_state()->link == GPS_LINK_RUN && mod_saved);
    size_t saves_before = 0;
    for (const char *q = written; (q = strstr(q, "PCAS00")); q++) saves_before++;
    uint32_t keep = mod_period_ms;
    mod_period_ms = 0;                              /* unplugged / reset        */
    run(GPS_LOST_MS + 100);
    CHECK(gps_state()->link == GPS_LINK_PROBE_FAST);
    mod_period_ms = keep;                           /* back, still at 115200    */
    run(1000);
    CHECK(gps_state()->link == GPS_LINK_RUN);
    size_t saves_after = 0;
    for (const char *q = written; (q = strstr(q, "PCAS00")); q++) saves_after++;
    CHECK(saves_after == saves_before);             /* flash written once       */
}

static void t_fix_fresh_and_decoded(void)
{
    reset();
    run(2000);
    CHECK(gps_fix_fresh(now_ms));
    const gps_state_t *g = gps_state();
    CHECK(fabsf(g->fix.speed_kmh - 27.0f * 1.852f) < 0.01f);
    CHECK(g->fix.sats == 9 && g->fix.quality == 1);
    mod_fix = false;                                /* tunnel                   */
    run(500);
    CHECK(!gps_fix_fresh(now_ms) && isnan(g->fix.speed_kmh));
    mod_fix = true;
    run(500);
    CHECK(gps_fix_fresh(now_ms));
    mod_period_ms = 0;                              /* silence                  */
    run(GPS_STALE_MS + 50);
    CHECK(!gps_fix_fresh(now_ms));                  /* old fix not shown        */
}

/* C7 SW-06: a fix is as old as its sentence's arrival, not its draining. A
 * loop stalled 1.9 s drains an RMC received at t0: with the drain time it
 * would stay "fresh" until t0 + 1.9 s + GPS_STALE_MS. */
static void t_c7_fix_time_is_receive_time(void)
{
    reset();
    run(2000);
    CHECK(gps_fix_fresh(now_ms));
    mod_period_ms = 0;
    uint32_t t0 = now_ms;
    mod_emit();                                     /* received at t0           */
    now_ms += 1900;                                 /* loop stuck, not drained  */
    gps_poll();
    const gps_state_t *g = gps_state();
    CHECK(g->fix_ms == t0 && g->rx_ms == t0);
    CHECK(gps_fix_fresh(t0 + GPS_STALE_MS - 1u));
    CHECK(!gps_fix_fresh(t0 + GPS_STALE_MS));
    CHECK(g->link == GPS_LINK_RUN);                 /* 1.9 s < GPS_LOST_MS      */
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "found_at_115200",             t_found_at_115200 },
        { "found_at_9600_switch_save",   t_found_at_9600_switched_and_saved },
        { "absent_retried_quietly",      t_absent_retried_quietly },
        { "lost_then_regained",          t_lost_then_regained_no_second_save },
        { "fix_fresh_and_decoded",       t_fix_fresh_and_decoded },
        { "c7_fix_time_is_receive_time", t_c7_fix_time_is_receive_time },
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = fails;
        reset();
        tests[i].fn();
        printf("%-30s %s\n", tests[i].name, fails == before ? "PASS" : "FAIL");
    }
    printf("\n%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
