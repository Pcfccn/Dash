/* The whole fdcan_obd suite again, built with OILP_DPID_ENABLE 0 (the
 * strict "$22 only" build): the TX mock then fails any $2C / $AA, and the
 * oil-pressure cases check that a refused $22 ends the search. */
#define OILP_DPID_ENABLE 0
#include "test_fdcan_obd.c"
