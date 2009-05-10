#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>

PJ_DECL(pj_status_t) pjmedia_silence_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, unsigned buffer_size, pjmedia_port **p_port);
