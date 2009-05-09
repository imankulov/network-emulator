#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>

PJ_DECL(pj_status_t) pjmedia_markov_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, double p10, double p00, pjmedia_port **p_port);
