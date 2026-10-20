/* test_alert.c again with OILP_VALIDATED 1: oil pressure is judged, but only
 * once the engine runs. The default (0) is test_alert.c itself. */
#define OILP_VALIDATED 1
#include "test_alert.c"
