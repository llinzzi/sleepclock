#ifndef SCREEN_MGR_H
#define SCREEN_MGR_H

#include <stdint.h>

typedef enum {
    SCREEN_SLEEP,
    SCREEN_WAKE,
    SCREEN_STAR,
    SCREEN_WEATHER,
} screen_mode_t;

void screen_mgr_init(void);
void screen_mgr_set_mode(screen_mode_t mode);
void screen_mgr_update(uint32_t elapsed_ms);
void screen_mgr_on_wake(void);
void screen_mgr_on_sleep(void);

#endif // SCREEN_MGR_H
