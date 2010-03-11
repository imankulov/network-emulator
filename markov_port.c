#include "markov_port.h"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('M', 'A', 'R', 'K')
#define THIS_FILE   "markov_port.c"

struct markov_port
{
    pjmedia_port	  base;
    pjmedia_port	 *dn_port;
    double            p10;
    double            p00;
    pj_bool_t         packet_lost;
};


static pj_status_t mp_put_frame(pjmedia_port *this_port,
				const pjmedia_frame *frame);
static pj_status_t mp_get_frame(pjmedia_port *this_port,
				pjmedia_frame *frame);
static pj_status_t mp_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_markov_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, double p10, double p00, pjmedia_port **p_port)
{
    const pj_str_t markov = { "markov", 6 };
    struct markov_port *mp;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);
    PJ_ASSERT_RETURN(p10 >=0 && p10 <=100, PJ_EINVAL);
    PJ_ASSERT_RETURN(p00 >=0 && p00 <=100, PJ_EINVAL);

    /* Create the port itself */
    mp = PJ_POOL_ZALLOC_T(pool, struct markov_port);

    pjmedia_port_info_init(&mp->base.info, &markov, SIGNATURE,
			   dn_port->info.clock_rate,
			   dn_port->info.channel_count,
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    /* More init */
    mp->dn_port = dn_port;
    mp->p10 = p10;
    mp->p00 = p00;
    mp->base.get_frame = &mp_get_frame;
    mp->base.put_frame = &mp_put_frame;
    mp->base.on_destroy = &mp_on_destroy;
    mp->packet_lost = PJ_FALSE;

    /* Done */
    *p_port = &mp->base;

    return PJ_SUCCESS;
}


static pj_status_t mp_put_frame( pjmedia_port *this_port,
				 const pjmedia_frame *frame)
{
    struct markov_port *mp = (struct markov_port*)this_port;
    double lost_threshold = mp->packet_lost ? mp->p00 : mp->p10;
    double rand;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    PJ_LOG(6, (THIS_FILE, "packet: sz=%d ts=%llu",
                frame->size/sizeof(pj_uint16_t), frame->timestamp.u64));
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE ) {
	    return pjmedia_port_put_frame(mp->dn_port, frame);
    }
    rand = (double)(pj_rand() / ((double)RAND_MAX+1.0) * 100.0);
    if (rand < lost_threshold) {
        pjmedia_frame tmp_frame;
        pj_bzero(&tmp_frame, sizeof(tmp_frame));
        tmp_frame.type = PJMEDIA_FRAME_TYPE_NONE;
        mp->packet_lost = 1;
        return pjmedia_port_put_frame(mp->dn_port, &tmp_frame);
    } else {
        mp->packet_lost = 0;
        return pjmedia_port_put_frame(mp->dn_port, frame);
    }
}


static pj_status_t mp_get_frame( pjmedia_port *this_port,
				 pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}



static pj_status_t mp_on_destroy(pjmedia_port *this_port)
{
    struct markov_port *mp = (struct markov_port*)this_port;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    return PJ_SUCCESS;
}



