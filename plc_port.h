#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
typedef enum {
    EM_PLC_EMPTY,
    EM_PLC_REPEAT,
    EM_PLC_NOISE,
    EM_PLC_SMART
} em_plc_mode;


typedef struct em_plc_statistics {
    pj_size_t received;
    pj_size_t lost;
    pj_size_t total;
} em_plc_statistics;

PJ_DECL(pj_status_t) pjmedia_plc_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, pjmedia_codec *codec, unsigned fpp,
        em_plc_mode plc_mode, pjmedia_port **p_port);


PJ_DECL(pj_status_t) pjmedia_plc_port_get_statistics(const pjmedia_port *port,
        em_plc_statistics *stats);
