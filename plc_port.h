#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
typedef enum {
    EM_PLC_EMPTY,
    EM_PLC_REPEAT,
    EM_PLC_NOISE,
    EM_PLC_SMART
} em_plc_mode;

PJ_DECL(pj_status_t) pjmedia_plc_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, pjmedia_codec *codec, unsigned fpp,
        em_plc_mode plc_mode, pjmedia_port **p_port);
