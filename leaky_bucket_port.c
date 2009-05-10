#include "leaky_bucket_port.h"
#define SIGNATURE   PJMEDIA_PORT_SIGNATURE('L', 'E', 'A', 'K')
#define THIS_FILE   "leaky_bucket_port.c"

struct leaky_bucket_item
{
    PJ_DECL_LIST_MEMBER(struct leaky_bucket_item);
    pjmedia_frame frame;
};

struct leaky_bucket_port
{
    pjmedia_port	   base;
    pjmedia_port	  *dn_port;
    pj_size_t          bucket_size;
    unsigned           sent_rate;
    struct leaky_bucket_item *first_item;/* first item to be pushed to dn_port */
    struct leaky_bucket_item *last_item; /* last item to be pushed to dn_port */
    pj_timestamp       last_ts;   /* timestamp when latest packet in the queue should be pushed */
    pj_size_t          items;     /* number of non-empty items in the bucket */
    pj_pool_t         *pool;
};


static pj_status_t lb_put_frame(pjmedia_port *this_port,
				const pjmedia_frame *frame);
static pj_status_t lb_get_frame(pjmedia_port *this_port,
				pjmedia_frame *frame);
static pj_status_t lb_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_leaky_bucket_port_create(
        pj_pool_factory *pool_factory, pjmedia_port *dn_port,
        pj_size_t bucket_size, unsigned sent_rate, pjmedia_port **p_port)
{
    const pj_str_t leaky_bucket = { "leaky", 5 };
    struct leaky_bucket_port *lb;
    pj_pool_t *pool;
    pj_status_t status;

    PJ_ASSERT_RETURN(pool_factory && dn_port && p_port, PJ_EINVAL);

    /* Create own memory pool */
    pool = pj_pool_create(pool_factory, "leaky", 4000, 4000, NULL);

    /* Create the port itself */
    lb = PJ_POOL_ZALLOC_T(pool, struct leaky_bucket_port);

    pjmedia_port_info_init(&lb->base.info, &leaky_bucket, SIGNATURE,
			   dn_port->info.clock_rate,
			   dn_port->info.channel_count,
			   dn_port->info.bits_per_sample,
			   dn_port->info.samples_per_frame);

    /* we have an empty list */
    lb->first_item = lb->last_item = NULL;
    lb->items = 0;

    /* More init */
    lb->dn_port = dn_port;
    lb->base.get_frame = &lb_get_frame;
    lb->base.put_frame = &lb_put_frame;
    lb->base.on_destroy = &lb_on_destroy;
    lb->bucket_size = bucket_size;
    lb->sent_rate = sent_rate;
    lb->pool = pool;
    lb->last_ts.u64 = 0;

    /* Done */
    *p_port = &lb->base;

    return PJ_SUCCESS;
}


static pj_status_t lb_push_frame(struct leaky_bucket_port *lb)
{
    struct leaky_bucket_item *fst = lb->first_item;
    pj_status_t status;
    if (fst == NULL){
        return PJ_SUCCESS;
    }
    status = pjmedia_port_put_frame(lb->dn_port, &fst->frame);
    if (status != PJ_SUCCESS)
        return status;
    if (fst->frame.type == PJMEDIA_FRAME_TYPE_AUDIO){
        lb->items --;
    } else {
    }
    if (fst->next == fst) {
        lb->first_item = lb->last_item = NULL;
    } else {
        lb->first_item = fst->next;
        pj_list_erase(fst);
    }
    return PJ_SUCCESS;
}



static pj_status_t lb_push_frames_till(struct leaky_bucket_port *lb,
        pj_timestamp ts)
{
    pj_status_t status;
    while (lb->first_item){
        if (lb->first_item->frame.type == PJMEDIA_FRAME_TYPE_AUDIO &&
                lb->first_item->frame.timestamp.u64 >= ts.u64)
            break;
        status = lb_push_frame(lb);
        if (status != PJ_SUCCESS)
            return status;
    };
    return PJ_SUCCESS;
}



static pj_status_t lb_put_frame( pjmedia_port *this_port,
				 const pjmedia_frame *frame)
{
    pj_status_t status;
    struct leaky_bucket_port *lb = (struct leaky_bucket_port*)this_port;
    struct leaky_bucket_item *item = PJ_POOL_ZALLOC_T(lb->pool,
            struct leaky_bucket_item);
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    pj_memcpy(&item->frame, frame, sizeof(pjmedia_frame));

    /* if frame is not empty, push all previous frames into downstream port  */
    if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO){
        status = lb_push_frames_till(lb, frame->timestamp);
        if (status != PJ_SUCCESS) return status;
    }
    /* update bucket and received packet states */
    if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO ) {
        if (lb->bucket_size > lb->items){
            void *buf;
            if ( lb->last_ts.u64 + lb->sent_rate < item->frame.timestamp.u64  ) {
                lb->last_ts.u64 = item->frame.timestamp.u64;
            } else {
                lb->last_ts.u64 += lb->sent_rate;
                item->frame.timestamp.u64 = lb->last_ts.u64;
            }
            buf = pj_pool_zalloc(lb->pool, frame->size);
            pj_memcpy(buf, frame->buf, frame->size);
            item->frame.buf = buf;
            lb->items++;
        } else {
            pj_bzero(item, sizeof(*item));
            item->frame.type = PJMEDIA_FRAME_TYPE_NONE;
        }
    } else {
    }
    /* put frame at the end of list or init an empty one */
    if (lb->last_item == NULL){
        pj_list_init(item);
        lb->first_item = lb->last_item = item;
    } else {
        pj_list_insert_after(lb->last_item, item);
        lb->last_item = item;
    }
    return PJ_SUCCESS;
}


static pj_status_t lb_get_frame( pjmedia_port *this_port,
				 pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(this_port);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP;
}



static pj_status_t lb_on_destroy(pjmedia_port *this_port)
{
    pj_status_t status;
    struct leaky_bucket_port *lb = (struct leaky_bucket_port*)this_port;
    PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE, PJ_EINVAL);
    while (lb->first_item){
        status = lb_push_frame(lb);
        if (status != PJ_SUCCESS)
            return status;
    };
    return PJ_SUCCESS;
}
