#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>

PJ_DECL(pj_status_t) pjmedia_leaky_bucket_port_create(
        pj_pool_factory *pool_factory, pjmedia_port *dn_port,
        pj_size_t bucket_size, unsigned sent_rate, pjmedia_port **p_port);

