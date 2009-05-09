#include "plc_port.h"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('P', 'L', 'C', 'P')
#define THIS_FILE   "plc_port.c"
#define BUF_SIZE    1024
#define MAX_FPP     10

struct plc_port
{
    pjmedia_port	  base;
    pjmedia_port	 *dn_port;
    pjmedia_codec    *codec;
    unsigned          fpp;
    em_plc_mode       plc_mode;
    pjmedia_frame     frame;
    void             *frame_buf;
};


static pj_status_t plc_put_frame(pjmedia_port *this_port,
				const pjmedia_frame *frame);
static pj_status_t plc_get_frame(pjmedia_port *this_port,
				pjmedia_frame *frame);
static pj_status_t plc_on_destroy(pjmedia_port *this_port);


PJ_DECL(pj_status_t) pjmedia_plc_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, pjmedia_codec *codec, unsigned fpp,
        em_plc_mode plc_mode, pjmedia_port **p_port)
{
    const pj_str_t plc = { "plc", 3 };
    struct plc_port *plcp;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);

    /* Create the port itself */
    plcp = PJ_POOL_ZALLOC_T(pool, struct plc_port);
    plcp->frame_buf = pj_pool_zalloc(pool, BUF_SIZE);

    pjmedia_port_info_init(&plcp->base.info, &plc, SIGNATURE,
			   dn_port->info.clock_rate,
			   dn_port->info.channel_count,
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    /* More init */
    plcp->dn_port = dn_port;
    plcp->codec = codec;
    plcp->fpp = fpp;
    plcp->plc_mode = plc_mode;
    plcp->base.get_frame = &plc_get_frame;
    plcp->base.put_frame = &plc_put_frame;
    plcp->base.on_destroy = &plc_on_destroy;
    plcp->frame.type = PJMEDIA_FRAME_TYPE_NONE;
    plcp->frame.buf = plcp->frame_buf;

    /* Done */
    *p_port = &plcp->base;

    return PJ_SUCCESS;
}


static pj_status_t plc_put_frame( pjmedia_port *this_port,
				 const pjmedia_frame *frame)
{
    pj_status_t status;
    int i;
    struct plc_port *plcp = (struct plc_port*)this_port;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);

    if (frame->type == PJMEDIA_FRAME_TYPE_NONE ) {
        em_plc_mode mode = plcp->frame.type == PJMEDIA_FRAME_TYPE_NONE ? \
            EM_PLC_EMPTY : plcp->plc_mode;
        switch (mode) {
            case EM_PLC_SMART:
            for (i=0; i<plcp->fpp; i++){
                status = plcp->codec->op->recover(plcp->codec, BUF_SIZE, &plcp->frame);
                if (status != PJ_SUCCESS) return status;
                status = pjmedia_port_put_frame(plcp->dn_port, &plcp->frame);
                if (status != PJ_SUCCESS) return status;
            }
            break;
            case EM_PLC_REPEAT:
            for (i=0; i<plcp->fpp; i++){
                status = pjmedia_port_put_frame(plcp->dn_port, &plcp->frame);
                if (status != PJ_SUCCESS) return status;
            }
            break;
            case EM_PLC_NOISE:
            for (i=0; i<plcp->fpp; i++){
                int j=0;
                plcp->frame.size = plcp->dn_port->info.bytes_per_frame / plcp->fpp;
                plcp->frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                for  (j=0; j<plcp->frame.size; j++)
                    ((char*)plcp->frame.buf)[j] = ((char)pj_rand()) >> 5;
                status = pjmedia_port_put_frame(plcp->dn_port, &plcp->frame);
                if (status != PJ_SUCCESS) return status;
            }
            default:
            for (i=0; i<plcp->fpp; i++){
                plcp->frame.size = plcp->dn_port->info.bytes_per_frame / plcp->fpp;
                plcp->frame.type = PJMEDIA_FRAME_TYPE_AUDIO;
                pj_bzero(plcp->frame.buf, plcp->frame.size);
                status = pjmedia_port_put_frame(plcp->dn_port, &plcp->frame);
            }
        }
    } else {
        unsigned cnt = MAX_FPP;
        pjmedia_frame out_frames[MAX_FPP];
        status = plcp->codec->op->parse(plcp->codec, frame->buf, frame->size,
                &frame->timestamp, &cnt, out_frames);
        for (i=0; i<cnt; i++){
            status = plcp->codec->op->decode(plcp->codec, &out_frames[i], BUF_SIZE,
                    &plcp->frame);
            if (status != PJ_SUCCESS) return status;
            status = pjmedia_port_put_frame(plcp->dn_port, &plcp->frame);
            if (status != PJ_SUCCESS) return status;
        }
    }
    return PJ_SUCCESS;
}


static pj_status_t plc_get_frame( pjmedia_port *this_port,
				 pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}



static pj_status_t plc_on_destroy(pjmedia_port *this_port)
{
    struct plc_port *plcp = (struct plc_port*)this_port;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    return PJ_SUCCESS;
}



