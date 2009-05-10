#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <stdio.h>
#include <string.h>

#include "markov_port.h"
#include "plc_port.h"

#define MAX_FPP 10


const char *input_file;
const char *output_file;
const char *codec_name;
double markov_p00;
double markov_p10;
unsigned fpp;
unsigned speex_quality;
unsigned log_level;
pj_bool_t list_codecs;
em_plc_mode plc_mode;


static void err(const char *op, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];
    pj_strerror(status, errmsg, sizeof(errmsg));
    fprintf(stderr, "%s error: %s\n", op, errmsg);
}

#define CHECK(op)   do { \
			status = op; \
			if (status != PJ_SUCCESS) { \
			    err(#op, status); \
			    return status; \
			} \
		    } \
		    while (0)


pj_status_t parse_args(int argc, const char *argv[])
{

    int i = 1;
    input_file = NULL;
    output_file = NULL;
    codec_name = NULL;
    markov_p00 = 0;
    markov_p10 = 0;
    fpp = 1;
    speex_quality = 8;
    log_level = 1;
    plc_mode = EM_PLC_EMPTY;
    list_codecs = PJ_FALSE;
    while ( i < argc ) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--input-file") || !strcmp(arg, "-i")){
            if (i != argc - 1 ) input_file = argv[++i];
        } else if (!strcmp(arg, "--output-file") ||  !strcmp(arg, "-o")) {
            if (i != argc - 1 ) output_file = argv[++i];
        } else if (!strcmp(arg, "--codec") || !strcmp(arg, "-c")) {
            if (i != argc - 1 ) codec_name = argv[++i];
        } else if (!strcmp(arg, "--fpp") || !strcmp(arg, "-f")) {
            if (i != argc - 1 ){
                fpp = atoi(argv[++i]);
                if (fpp < 1 || fpp > MAX_FPP){
                    fprintf(stderr, "fpp must be between 1 and %d", MAX_FPP);
                    goto err;
                }
            }
        } else if (!strcmp(arg, "--loss") || !strcmp(arg, "-l")) {
            if (i != argc - 1 ) {
                markov_p10 = markov_p00 = atof(argv[++i]);
            }
        } else if (!strcmp(arg, "--p00"))  {
            if (i != argc - 1 ) {
                markov_p00 = atof(argv[++i]);
            }
        } else if (!strcmp(arg, "--p10")) {
            if (i != argc - 1 ) {
                markov_p10 = atof(argv[++i]);
            }
        } else if (!strcmp(arg, "--speex-quality") || !strcmp(arg, "-q")) {
            if (i != argc - 1 ) {
                speex_quality = atoi(argv[++i]);
            }
        } else if (!strcmp(arg, "--log-level")) {
            if (i != argc - 1 ) {
                log_level = atoi(argv[++i]);
                if (log_level < 0 || log_level > 6){
                    fprintf(stderr, "Log level must be in [0..6]\n");
                    goto err;
                }
            }
        } else if (!strcmp(arg, "--plc") || !strcmp(arg, "-p")) {
            if (i != argc - 1 ) {
                const char *plc_s = argv[++i];
                switch (plc_s[0]){
                    case 'e': plc_mode = EM_PLC_EMPTY; break;
                    case 'r': plc_mode = EM_PLC_REPEAT; break;
                    case 'n': plc_mode = EM_PLC_NOISE; break;
                    case 's': plc_mode = EM_PLC_SMART; break;
                    default:
                        fprintf(stderr, "Unknown argument for PLC: %s\n", plc_s);
                        goto err;
                }
            }
        } else if (!strcmp(arg, "--list-codecs")) {
            list_codecs = PJ_TRUE;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", arg);
            goto err;
        }
        i++;
    }
    if (list_codecs)
        return PJ_SUCCESS;
    if (!input_file || !output_file || !codec_name)
        goto err;
    return  PJ_SUCCESS;

err:
    fprintf(stderr, "Usage: %s -i|--input-file <filename1.wav>\n", argv[0]);
    fprintf(stderr, "          -o|--output-file <filename2.wav>\n");
    fprintf(stderr, "          -c|--codec <CODEC_NAME>\n");
    fprintf(stderr, "          -l|--loss <lost_pct>\n");
    fprintf(stderr, "             --p00 <lost_pct>\n");
    fprintf(stderr, "             --p10 <lost_pct>\n");
    fprintf(stderr, "          -f|--fpp <fpp>\n");
    fprintf(stderr, "          -p|--plc empty|repeat|smart|noise\n");
    fprintf(stderr, "          -q|--speex-quality <value>\n");
    fprintf(stderr, "             --log-level <0..6>\n");
    fprintf(stderr, "             --list-codecs\n");
    return 1;
}


int main(int argc, const char *argv[])
{
    pj_caching_pool cp;
    pj_pool_t *pool;
    pjmedia_codec_mgr *cm;
    pjmedia_codec *codec;
    pjmedia_endpt *med_endpt;
    pjmedia_port *rec_file_port = NULL, *play_file_port = NULL,
        *markov_port = NULL, *plc_port = NULL;
    pjmedia_master_port *master_port = NULL;
    pj_status_t status;
    pjmedia_frame pcm_frame, frame, out_frames[MAX_FPP];
    const pjmedia_codec_info *codec_info;
    pjmedia_codec_param codec_param;
    pj_str_t tmp;
    void *pcm_buf = NULL, *buf = NULL, *tmp_buf = NULL;
    unsigned codec_count = 1;
    pj_size_t buf_size = 0, real_buf_size = 0;
    int packet_lost = 0;
    pj_timestamp read_ts;

    status = parse_args(argc, argv);
    if (status != PJ_SUCCESS)
        return status;
    pj_log_set_level(log_level);
    status = pj_init();
    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);
    pool = pj_pool_create(&cp.factory, "emulator", 4000, 4000, NULL);
    CHECK (pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt));
#if PJMEDIA_HAS_G711_CODEC
    CHECK (pjmedia_codec_g711_init(med_endpt));
#endif
#if PJMEDIA_HAS_GSM_CODEC
    CHECK (pjmedia_codec_gsm_init(med_endpt));
#endif
#if PJMEDIA_HAS_ILBC_CODEC
    CHECK (pjmedia_codec_ilbc_init(med_endpt, 30));
#endif
#if PJMEDIA_HAS_SPEEX_CODEC
    CHECK (pjmedia_codec_speex_init(med_endpt, 0, speex_quality,
            PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY));
#endif
#if PJMEDIA_HAS_G722_CODEC
    CHECK (pjmedia_codec_g722_init(med_endpt));
#endif
#if PJMEDIA_HAS_INTEL_IPP
    CHECK (pjmedia_codec_ipp_init(med_endpt));
#endif
    cm = pjmedia_endpt_get_codec_mgr(med_endpt);
    CHECK( (cm ? PJ_SUCCESS : -1) );
    if (list_codecs) {
        unsigned codec_count = 128;
        pjmedia_codec_info codec_info_set[128];
        int i;
        CHECK( pjmedia_codec_mgr_enum_codecs(cm, &codec_count, codec_info_set,
                    NULL));
        printf("Found %d codecs: \n", codec_count);
        for (i=0; i<codec_count; i++){
            pjmedia_codec_info ci = codec_info_set[i];
            printf(" - %.*s/%u/%u\n",
                    (int)ci.encoding_name.slen, ci.encoding_name.ptr,
                    ci.clock_rate, ci.channel_cnt);
        }
        exit (0);
    }
    CHECK (pjmedia_codec_mgr_find_codecs_by_id(cm,
            pj_cstr(&tmp, codec_name), &codec_count, &codec_info, NULL) );
    CHECK (pjmedia_codec_mgr_get_default_param(cm, codec_info, &codec_param));
    codec_param.setting.vad = 0;
    codec_param.setting.cng = 0;

    CHECK (pjmedia_codec_mgr_alloc_codec(cm, codec_info, &codec));
    CHECK (codec->op->init(codec, pool) );
    CHECK (codec->op->open(codec, &codec_param));
    CHECK (pjmedia_wav_player_port_create(pool, input_file,
            codec_param.info.frm_ptime*fpp, PJMEDIA_FILE_NO_LOOP, 0,
            &play_file_port));
    buf_size = play_file_port->info.bytes_per_frame;
    pcm_buf = pj_pool_zalloc(pool, buf_size);
    buf = pj_pool_zalloc(pool, buf_size);
    tmp_buf = pj_pool_zalloc(pool, buf_size/fpp);
    CHECK( ( buf && pcm_buf && tmp_buf ? PJ_SUCCESS : -1) );
    CHECK(pjmedia_wav_writer_port_create(pool, output_file,
            play_file_port->info.clock_rate,
            play_file_port->info.channel_count,
            play_file_port->info.samples_per_frame,
            play_file_port->info.bits_per_sample, 0, 0, &rec_file_port));
    CHECK(pjmedia_plc_port_create(pool, rec_file_port, codec, fpp, plc_mode,
                &plc_port));
    CHECK(pjmedia_markov_port_create(pool, plc_port, markov_p10,
                markov_p00, &markov_port));
    #if 0
    status = pjmedia_master_port_create(pool, play_file_port, rec_file_port,
            0, &master_port);
    status = pjmedia_master_port_start(master_port);
    while (!stop) {
        status = pj_thread_sleep(10);
    }
    status = pjmedia_master_port_destroy(master_port, PJ_TRUE);
    #endif
    read_ts.u64 = 0;
    for(;;){
        int i;
        unsigned cnt = MAX_FPP;
        double lost_threshold = 0;

        pcm_frame.buf = pcm_buf;
        pcm_frame.size = buf_size;
        status = pjmedia_port_get_frame(play_file_port, &pcm_frame);
        if (status != PJ_SUCCESS || pcm_frame.type == PJMEDIA_FRAME_TYPE_NONE) break;
        pcm_frame.timestamp.u64 = read_ts.u64;
        frame.buf = buf;
        frame.size = buf_size;
        CHECK (codec->op->encode(codec, &pcm_frame, buf_size, &frame));

        CHECK(pjmedia_port_put_frame(markov_port, &frame));
        read_ts.u64 += 1e6 * play_file_port->info.samples_per_frame / play_file_port->info.clock_rate;
    }
    pjmedia_port_destroy(play_file_port);
    pjmedia_port_destroy(rec_file_port);
}
