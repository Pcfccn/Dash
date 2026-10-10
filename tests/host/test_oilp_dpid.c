/* The whole fdcan_obd suite again, built as the KOEO experiment build
 * (OILP_DPID_ENABLE 1, the $2C/$AA path for oil pressure). The default build
 * (0) is test_fdcan_obd.c itself, where the TX mock fails any $2C / $AA. */
#define OILP_DPID_ENABLE 1
#include "test_fdcan_obd.c"
