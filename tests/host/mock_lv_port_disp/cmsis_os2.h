/* Host-side stand-in: the one CMSIS-RTOS2 call lv_port_disp.c uses. */
#ifndef MOCK_CMSIS_OS2_H
#define MOCK_CMSIS_OS2_H
#include <stdint.h>
uint32_t osKernelGetTickCount(void);
#endif
