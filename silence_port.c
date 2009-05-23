#include "silence_port.h"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('S', 'I', 'L', 'E')
#define THIS_FILE   "silence_port.c"

struct silence_port
{
    pjmedia_port	  base;
    pjmedia_port	 *dn_port;
    pj_timestamp      last_ts;
    unsigned          frames;
    pjmedia_circ_buf *buf;
};


static pj_status_t sp_put_frame(pjmedia_port *this_port,
				const pjmedia_frame *frame);
static pj_status_t sp_get_frame(pjmedia_port *this_port,
				pjmedia_frame *frame);
static pj_status_t sp_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_silence_port_create(pj_pool_t *pool,
        pjmedia_port *dn_port, unsigned buffer_size, pjmedia_port **p_port)
{
    const pj_str_t silence = { "silence", 7 };
    struct silence_port *sp;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool && dn_port && p_port, PJ_EINVAL);

    /* Create the port itself */
    sp = PJ_POOL_ZALLOC_T(pool, struct silence_port);

    pjmedia_port_info_init(&sp->base.info, &silence, SIGNATURE,
			   dn_port->info.clock_rate,
			   dn_port->info.channel_count,
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    /* More init */
    sp->dn_port = dn_port;
    sp->base.get_frame = &sp_get_frame;
    sp->base.put_frame = &sp_put_frame;
    sp->base.on_destroy = &sp_on_destroy;
    sp->frames = 0;
    sp->last_ts.u64 = 0;

    /* create circular buffer */
    if (!buffer_size)
        buffer_size = dn_port->info.bits_per_sample * \
                      dn_port->info.samples_per_frame * 16;
    pjmedia_circ_buf_create (pool, buffer_size, &sp->buf);

    /* Done */
    *p_port = &sp->base;

    return PJ_SUCCESS;
}


static pj_status_t sp_put_frame( pjmedia_port *this_port,
				 const pjmedia_frame *frame)
{
    struct silence_port *sp = (struct silence_port*)this_port;
    unsigned frame_size = frame->size / sizeof(pj_uint16_t);
    unsigned samples_per_frame = sp->base.info.samples_per_frame;
    pj_int16_t tmp_buf[frame_size];
    pjmedia_frame tmp_frame;
    pj_status_t status;
    PJ_LOG(6, (THIS_FILE, "packet: sz=%d ts=%llu",
                frame->size/sizeof(pj_uint16_t), frame->timestamp.u64));
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    if (frame->type == PJMEDIA_FRAME_TYPE_NONE ) {
        PJ_LOG(5, (THIS_FILE, "empty frame passed"));
	    return pjmedia_port_put_frame(sp->dn_port, frame);
    }

    if (sp->frames == 0) {
        PJ_LOG(5, (THIS_FILE, "%u: this is first received frame,  "
                    "so just update last timestamp with its "
                    "timestamp + size: %llu + %d",
                    sp->frames,
                    frame->timestamp.u64,
                    samples_per_frame));
        sp->last_ts.u64 = frame->timestamp.u64 + samples_per_frame;
    } else if (frame->timestamp.u64 == 0){
        PJ_LOG(5, (THIS_FILE, "%u: frame timestamp is unknown, so increment "
                    "by %d (samples per frame)", sp->frames, samples_per_frame));
        sp->last_ts.u64 += samples_per_frame;
    } else if (frame->timestamp.u64 < sp->last_ts.u64 ){
        PJ_LOG(5, (THIS_FILE, "%u: received frame timestamp is lesser than "
                    "latest timestamp (%llu < %llu)", sp->frames,
                    frame->timestamp.u64, sp->last_ts.u64));
        sp->last_ts.u64 = frame->timestamp.u64 + samples_per_frame;
    } else if (frame->timestamp.u64 > sp->last_ts.u64){
        unsigned zero_padding_count = frame->timestamp.u64 - sp->last_ts.u64;
        pj_int16_t zero_buf[zero_padding_count];
        PJ_LOG(5, (THIS_FILE, "%u: received frame timestamp is greater than "
                "latest timestamp (%llu > %llu), fill output buffer with %d "
                "empty frames",
                sp->frames,
                frame->timestamp.u64,
                sp->last_ts.u64,
                zero_padding_count));
        pj_bzero(zero_buf, sizeof(zero_buf));
        pjmedia_circ_buf_write(sp->buf, zero_buf, zero_padding_count);
        sp->last_ts.u64 = frame->timestamp.u64 + samples_per_frame;
    } else {
        sp->last_ts.u64 += samples_per_frame;
    }
    pjmedia_circ_buf_write(sp->buf, (pj_uint16_t*)frame->buf, frame_size);
    pj_memcpy(&tmp_frame, frame, sizeof(pjmedia_frame));
    tmp_frame.buf = (void*)tmp_buf;
    sp->frames ++;

    /* put everything saved into buffer */
    while (pjmedia_circ_buf_get_len(sp->buf) > frame_size){
        pjmedia_circ_buf_read(sp->buf, tmp_buf, frame_size);
        status = pjmedia_port_put_frame(sp->dn_port, &tmp_frame);
        if (status != PJ_SUCCESS)
            return status;
    }
    return PJ_SUCCESS;
}


static pj_status_t sp_get_frame( pjmedia_port *this_port,
				 pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}



static pj_status_t sp_on_destroy(pjmedia_port *this_port)
{
    struct silence_port *sp = (struct silence_port*)this_port;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    return PJ_SUCCESS;
}



