#ifndef __TOUCH_H
#define __TOUCH_H

#include "main.h"

#define CT_MAX_TOUCH 5
#define TP_PRES_DOWN 0x8000
#define TP_CATH_PRES 0x4000

typedef struct {
    uint16_t x[CT_MAX_TOUCH];
    uint16_t y[CT_MAX_TOUCH];
    uint16_t sta;
    uint8_t touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

#endif // !__TOUCH_H
