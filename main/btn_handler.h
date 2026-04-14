#ifndef BTN_HANDLER_H
#define BTN_HANDLER_H

#include <stdint.h>

typedef enum {
    BTN_EVENT_NONE,
    BTN_EVENT_PRESS,
    BTN_EVENT_LONG_3S,
    BTN_EVENT_LONG_10S,
} btn_event_t;

void btn_handler_init(void);
btn_event_t btn_handler_poll(void);

#endif // BTN_HANDLER_H
