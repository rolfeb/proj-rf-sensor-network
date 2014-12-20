#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

typedef uint16_t        clock_time_t;

extern volatile uint8_t msg_pending;
extern volatile uint8_t msg_error;
extern char             msg_buffer[];

extern void             wireless_init
                        (
                            volatile uint8_t *ddr,
                            volatile uint8_t *porto,
                            volatile uint8_t *porti,
                            uint8_t pin
                        );

extern void             clock_init(void);
extern clock_time_t     clock_time(void);
extern clock_time_t     clock_time_unlocked(void);

#endif /* __COMMON_H__ */
