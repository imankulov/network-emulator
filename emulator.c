#include <pjlib.h>
#include <pjlib-util.h>
#include <pjmedia.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pjmedia-codec/speex.h>

#include "markov_port.h"
#include "plc_port.h"
#include "silence_port.h"
#include "leaky_bucket_port.h"

#define MAX_FPP 10
#define THIS_FILE   "emulator.c"
#define em_set(x)   ((x)>=0)
#define em_unset(x) ((x)<0)

extern const pj_uint16_t pjmedia_codec_amrnb_bitrates[8];
extern const pj_uint16_t pjmedia_codec_amrwb_bitrates[9];

extern char *optarg;

const char *input_file;
const char *output_file;
const char *codec_name;
double markov_p00;
double markov_p10;
double lost_pct;
double burst_ratio;
unsigned fpp;
unsigned speex_quality;
#ifdef PJMEDIA_SPEEX_HAS_VBR
float speex_vbr_quality;
int speex_abr_bitrates[3];
#endif
unsigned log_level;
unsigned codec_bitrate;
pj_bool_t list_codecs;
em_plc_mode plc_mode;
pj_size_t bucket_size;
unsigned sent_delay;
double bits_per_second;
double packets_per_second;
pj_bool_t show_stats;

enum {
    EM_P00 = 1,
    EM_P10,
    EM_BUCKET_SIZE,
    EM_BURST_RATIO,
    EM_BANDWIDTH,
    EM_SENT_DELAY,
    EM_SHOW_STATS,
    EM_LOG_LEVEL,
    EM_LIST_CODECS,
} option_name;

#ifdef PJMEDIA_SPEEX_HAS_VBR
char shortopts[] = "i:c:q:Q:b:f:l:o:p:h";
#else
char shortopts[] = "i:c:q:b:f:l:o:p:h";
#endif
const struct pj_getopt_option longopts[] = {
    /* encoder options */
    {"input-file", required_argument, NULL, 'i'},
    {"codec", required_argument, NULL, 'c'},
    {"speex-quality", required_argument, NULL, 'q'},
#ifdef PJMEDIA_SPEEX_HAS_VBR
    {"speex-vbr-quality", required_argument, NULL, 'Q'},
#endif
    {"bitrate", required_argument, NULL, 'b'},
    {"fpp", required_argument, NULL, 'f'},

    /* channel options */
    {"loss", required_argument, NULL, 'l'},
    {"p00", required_argument, (int*)&option_name, (int)EM_P00},
    {"p10", required_argument, (int*)&option_name, (int)EM_P10},
    {"bucket-size", required_argument, (int*)&option_name, (int)EM_BUCKET_SIZE},
    {"burst-ratio", required_argument, (int*)&option_name, (int)EM_BURST_RATIO},
    {"bw", required_argument, (int*)&option_name, (int)EM_BANDWIDTH},
    {"bandwidth", required_argument, (int*)&option_name, (int)EM_BANDWIDTH},
    {"sent-delay", required_argument, (int*)&option_name, (int)EM_SENT_DELAY},

    /* decoder options */
    {"output-file", required_argument, NULL, 'o'},
    {"plc", required_argument, NULL, 'p'},

    /* miscellaneous options */
    {"show-stats", no_argument, (int*)&option_name, (int)EM_SHOW_STATS},
    {"log-level", required_argument, (int*)&option_name, (int)EM_LOG_LEVEL},
    {"list-codecs", no_argument, (int*)&option_name, (int)EM_LIST_CODECS},
    {"help", no_argument, NULL, 'h'},

    /* end */
    {NULL, 0, NULL, 0},
};



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
    markov_p00 = -1;
    markov_p10 = -1;
    lost_pct = -1;
    burst_ratio = -1;

    fpp = 1;
    speex_quality = 8;
#ifdef PJMEDIA_SPEEX_HAS_VBR
    speex_vbr_quality = -1;
    speex_abr_bitrates[0] = 0;
    speex_abr_bitrates[1] = 0;
    speex_abr_bitrates[2] = 0;
#endif
    log_level = 1;
    plc_mode = EM_PLC_EMPTY;
    list_codecs = PJ_FALSE;
    bucket_size = 50; /* 1s */
    sent_delay = 16; /* 10 times more than needed */
    bits_per_second = 0;
    packets_per_second = 0;
    codec_bitrate = 0;
    show_stats = PJ_FALSE;

    int ch;
    while ( (ch=getopt_long(argc, argv, shortopts, longopts, NULL)) != -1 ) {
        switch (ch) {
            case 'i':
                input_file = strdup(optarg);
                break;
            case 'c':
                codec_name = strdup(optarg);
                break;
            case 'q':
                speex_quality = atoi(optarg);
                if (speex_quality < 0 || speex_quality > 10){
                    fprintf(stderr, "speex quality must be between 0 and 10\n");
                    goto err;
                }
                break;
#ifdef PJMEDIA_SPEEX_HAS_VBR
            case 'Q':
                speex_vbr_quality = atof(optarg);
                if (speex_vbr_quality < 0 || speex_vbr_quality > 10){
                    fprintf(stderr, "speex VBR quality must be between 0 and 10\n");
                    goto err;
                }
                break;
#endif
            case 'b':
                codec_bitrate = atoi(optarg);
                break;
            case 'f':
                fpp = atoi(optarg);
                if (fpp < 1 || fpp > MAX_FPP){
                    fprintf(stderr, "fpp must be between 1 and %d", MAX_FPP);
                    goto err;
                }
                break;
            case 'l':
                lost_pct = atof(optarg);
                if (lost_pct < 0 || lost_pct > 100) {
                    fprintf(stderr, "loss percent must be between 0 and 100");
                    goto err;
                }
                break;
            case 'o':
                output_file = strdup(optarg);
                break;
            case 'p':
                switch (optarg[0]){
                    case 'e': plc_mode = EM_PLC_EMPTY; break;
                    case 'r': plc_mode = EM_PLC_REPEAT; break;
                    case 'n': plc_mode = EM_PLC_NOISE; break;
                    case 's': plc_mode = EM_PLC_SMART; break;
                    default:
                        fprintf(stderr, "Unknown argument for PLC: %s\n", optarg);
                        goto err;
                }
                break;
            case 'h':
                if (execlp("man", "man", "emulator", NULL)) {
                    fprintf(stderr, "Cannot display emulator man page\n");
                    goto err;
                }
            case 0:
                switch (option_name) {
                    case EM_P00:
                        markov_p00 = atof(optarg);
                        if (markov_p00 < 0 || markov_p00 > 100) {
                            fprintf(stderr, "loss rate must be between 0 and 100");
                            goto err;
                        }
                        break;
                    case EM_P10:
                        markov_p10 = atof(optarg);
                        if (markov_p10 < 0 || markov_p10 > 100) {
                            fprintf(stderr, "loss rate must be between 0 and 100");
                            goto err;
                        }
                        break;
                    case EM_BURST_RATIO:
                        burst_ratio = atof(optarg);
                        if (burst_ratio <= 0) {
                            fprintf(stderr, "burst ratio be greater than 0");
                            goto err;
                        }
                        break;
                    case EM_BUCKET_SIZE:
                        bucket_size = atoi(optarg);
                        break;
                    case EM_BANDWIDTH: {
                        int rlen = strlen(optarg);
                        if (rlen > 3 && strcmp(&optarg[rlen-3], "bps") == 0) {
                            bits_per_second = atof(optarg);
                            if (optarg[rlen-4] == 'k' || optarg[rlen-4] == 'K')
                                bits_per_second *= 1024;
                            else if (optarg[rlen-4] == 'm' || optarg[rlen-4] == 'M')
                                bits_per_second *= 1024*1024;
                            packets_per_second = 0;
                            sent_delay = 0;
                        } else if (rlen > 3 && strcmp(&optarg[rlen-3], "pps") == 0) {
                            packets_per_second = atof(optarg);
                            if (optarg[rlen-4] == 'k' || optarg[rlen-4] == 'K')
                                packets_per_second *= 1024;
                            else if (optarg[rlen-4] == 'm' || optarg[rlen-4] == 'M')
                                packets_per_second *= 1024*1024;
                            sent_delay = 0;
                            bits_per_second = 0;
                        } else {
                            fprintf(stderr, "Bandwidth value must ends with "
                                    "\"bps\" or \"pps\" \n");
                            goto err;
                        }
                        }
                        break;
                    case EM_SENT_DELAY:
                        sent_delay = atoi(optarg);
                        bits_per_second = 0;
                        packets_per_second = 0;
                        break;
                    case EM_SHOW_STATS:
                        show_stats = PJ_TRUE;
                        break;
                    case EM_LOG_LEVEL:
                        log_level = atoi(optarg);
                        if (log_level < 0 || log_level > 6){
                            fprintf(stderr, "Log level must be in [0..6]\n");
                            goto err;
                        }
                        break;
                    case EM_LIST_CODECS:
                        list_codecs = PJ_TRUE;
                        break;
                    default:
                        fprintf(stderr, "Unknown argument : %d\n", option_name);
                        goto err;
                }
                break;
            default:
                fprintf(stderr, "Unknown argument : %c\n",  (char)ch);
                goto err;
        }

    }
    if (list_codecs)
        return PJ_SUCCESS;
    if (!input_file || !output_file || !codec_name)
        goto err;
    if (codec_bitrate > 0){
        if (strncmp (codec_name, "AMR-WB", 6) == 0) {
            pj_bool_t bitrate_found = PJ_FALSE;
            int bitrates = 9;
            for (i=0; i<bitrates; i++)
                if (pjmedia_codec_amrwb_bitrates[i] == codec_bitrate)
                    bitrate_found = PJ_TRUE;
            if (!bitrate_found){
                fprintf(stderr, "Wrong bitrate for AMR-WB: %d\n", codec_bitrate);
                fprintf(stderr, "Acceptable values are: ");
                for (i=0; i<bitrates; i++)
                    fprintf(stderr, "%d ", pjmedia_codec_amrwb_bitrates[i]);
                fprintf(stderr, "\n");
                goto err;
            }
        } else if (strncmp (codec_name, "AMR", 3) == 0) {
            pj_bool_t bitrate_found = PJ_FALSE;
            int bitrates = 8;

            for (i=0; i<bitrates; i++)
                if (pjmedia_codec_amrnb_bitrates[i] == codec_bitrate)
                    bitrate_found = PJ_TRUE;
            if (!bitrate_found){
                fprintf(stderr, "Wrong bitrate for AMR: %d\n", codec_bitrate);
                fprintf(stderr, "Acceptable values are: ");
                for (i=0; i<bitrates; i++)
                    fprintf(stderr, "%d ", pjmedia_codec_amrnb_bitrates[i]);
                fprintf(stderr, "\n");
                goto err;
            }
        } else if (strncmp (codec_name, "G723", 4) == 0) {
            if (codec_bitrate != 6300 && codec_bitrate != 5300){
                fprintf(stderr, "Wrong bitrate for G.723: %d\n",
                        codec_bitrate);
                fprintf(stderr, "Acceptable values are: 5300, 6300\n");
                goto err;
            }
#ifdef PJMEDIA_SPEEX_HAS_VBR
        } else if (strncmp (codec_name, "speex", 5) == 0) {
            speex_abr_bitrates[0] = codec_bitrate;
            speex_abr_bitrates[1] = codec_bitrate;
            speex_abr_bitrates[2] = codec_bitrate;
#endif
        } else {
            fprintf(stderr, "`bitrate' option is not acceptable "
                    "for this codec\n");
            goto err;
        }
    }
    /* check and set up loss rates (given from G.107 p.9) */
    if (em_set(markov_p10) && em_set(markov_p00)){
        if (em_set(lost_pct) || em_set(burst_ratio)){
            fprintf(stderr, "Incompatible p.. variables. "
                "You must set up one or two\n");
            goto err;
        }
    } else if (em_set(lost_pct)) {
        if (em_unset(burst_ratio)){
            markov_p10 = markov_p00 = lost_pct;
        } else if (em_set(markov_p00) || em_set(markov_p10)) {
            fprintf(stderr, "Incompatible p.. variables. "
                "You must set up one or two\n");
            goto err;
        } else {
            markov_p10 = lost_pct / burst_ratio;
            /*
            markov_p00 = 100 * (1.0 - markov_p10/lost_pct) + markov_p10;
            markov_p00 = 100 - 100/burst_ratio + lost_pct/burst_ratio;
            */
            markov_p00 = 100.0 - (100.0 - lost_pct)/burst_ratio;
        }
    } else if ( em_unset(markov_p10) && em_unset(markov_p00) &&
            em_unset(lost_pct) && em_unset(burst_ratio) ) {
        markov_p10 = markov_p00 = 0.00;
    } else {
        fprintf(stderr, "Incompatible p.. variables. "
                "You must set up one or two\n");
        goto err;
    }
    PJ_LOG(5, (THIS_FILE, "channel loss properties: p00=%.2f, p10=%.2f "
                "lost_pct=%.2f burst=%.2f", markov_p00, markov_p10, lost_pct,
                burst_ratio));

    return  PJ_SUCCESS;

err:
    fprintf(stderr, "Usage: %s -i|--input-file <filename1.wav>\n", argv[0]);
    fprintf(stderr, "          -o|--output-file <filename2.wav>\n");
    fprintf(stderr, "          -c|--codec <CODEC_NAME>\n");
    fprintf(stderr, "          -b|--bitrate <CODEC_BITRATE> \n"
                    "               (see man for acceptable values)\n");
    fprintf(stderr, "          -l|--loss <lost_pct>\n");
    fprintf(stderr, "             --p00 <lost_pct>\n");
    fprintf(stderr, "             --p10 <lost_pct>\n");
    fprintf(stderr, "             --burst-ratio <ratio>\n");
    fprintf(stderr, "          -f|--fpp <fpp>\n");
    fprintf(stderr, "          -p|--plc empty|repeat|smart|noise\n");
    fprintf(stderr, "          -q|--speex-quality <value>\n");
#ifdef PJMEDIA_SPEEX_HAS_VBR
    fprintf(stderr, "          -Q|--speex-vbr-quality <value>\n");
#endif
    fprintf(stderr, "             --log-level <0..6>\n");
    fprintf(stderr, "             --bucket-size <n>\n");
    fprintf(stderr, "             --sent-delay <n>\n");
    fprintf(stderr, "        --bw|--bandwidth Abps|Bpps\n");
    fprintf(stderr, "             --show-stats\n");
    fprintf(stderr, "OR                       \n");
    fprintf(stderr, "       %s --list-codecs\n", argv[0]);
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
        *markov_port = NULL, *leaky_bucket_port = NULL,
        *silence_port = NULL, *plc_port = NULL;
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
    pj_uint32_t total_bytes = 0; /* transmitted throught network interface (raw) */

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
    #ifdef PJMEDIA_SPEEX_HAS_VBR
    CHECK (pjmedia_codec_speex_vbr_init(med_endpt, 0, speex_quality,
            PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY, speex_vbr_quality,
            speex_abr_bitrates));
    #else
    CHECK (pjmedia_codec_speex_init(med_endpt, 0, speex_quality,
            PJMEDIA_CODEC_SPEEX_DEFAULT_COMPLEXITY));
    #endif
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
            printf(" - %.*s/%u/%u\t",
                    (int)ci.encoding_name.slen, ci.encoding_name.ptr,
                    ci.clock_rate, ci.channel_cnt);
            CHECK (pjmedia_codec_mgr_get_default_param(cm, &ci, &codec_param));
            printf(" avg bps=%u\n", codec_param.info.avg_bps);
        }
        exit (0);
    }
    CHECK (pjmedia_codec_mgr_find_codecs_by_id(cm,
            pj_cstr(&tmp, codec_name), &codec_count, &codec_info, NULL) );
    CHECK (pjmedia_codec_mgr_get_default_param(cm, codec_info, &codec_param));
    codec_param.setting.vad = 0;
    codec_param.setting.cng = 0;
    if (plc_mode != EM_PLC_SMART)
        codec_param.setting.plc = 0;
    if (codec_bitrate > 0)
        codec_param.info.avg_bps = codec_bitrate;

    CHECK (pjmedia_codec_mgr_alloc_codec(cm, codec_info, &codec));
    CHECK (codec->op->init(codec, pool) );
    CHECK (codec->op->open(codec, &codec_param));
    PJ_LOG(5, (THIS_FILE, "created codec: clock_rate=%u, "
                "frm_ptime=%u, enc_ptime=%u, pcm_bits_per_sample=%u, pt=%u",
                (unsigned)codec_param.info.frm_ptime,
                (unsigned)codec_param.info.enc_ptime,
                (unsigned)codec_param.info.pcm_bits_per_sample,
                (unsigned)codec_param.info.pt
               ));

    CHECK (pjmedia_wav_player_port_create(pool, input_file,
            codec_param.info.frm_ptime*fpp, PJMEDIA_FILE_NO_LOOP, 0,
            &play_file_port));
    buf_size = play_file_port->info.bytes_per_frame;
    pcm_buf = pj_pool_zalloc(pool, buf_size);
    buf = pj_pool_zalloc(pool, buf_size);
    PJ_LOG(5, (THIS_FILE, "created buffer with size %u",
                (unsigned)play_file_port->info.bytes_per_frame
               ));

    tmp_buf = pj_pool_zalloc(pool, buf_size/fpp);
    CHECK( ( buf && pcm_buf && tmp_buf ? PJ_SUCCESS : -1) );
    CHECK(pjmedia_wav_writer_port_create(pool, output_file,
            play_file_port->info.clock_rate,
            play_file_port->info.channel_count,
            play_file_port->info.samples_per_frame/fpp,
            play_file_port->info.bits_per_sample, 0, 0, &rec_file_port));
    CHECK(pjmedia_silence_port_create(pool, rec_file_port, 0, &silence_port));
    CHECK(pjmedia_plc_port_create(pool, silence_port, codec, fpp, plc_mode,
                &plc_port));
    CHECK(pjmedia_leaky_bucket_port_create(&cp.factory, plc_port, bucket_size,
                sent_delay,
                (unsigned)bits_per_second,
                (unsigned)packets_per_second,
                &leaky_bucket_port));
    CHECK(pjmedia_markov_port_create(pool, leaky_bucket_port, markov_p10,
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
        if (status != PJ_SUCCESS || pcm_frame.type == PJMEDIA_FRAME_TYPE_NONE)
            break;
        pcm_frame.timestamp.u64 = read_ts.u64;
        PJ_LOG(6, (THIS_FILE, "pcm packet: sz=%d ts=%llu",
                pcm_frame.size/sizeof(pj_uint16_t), pcm_frame.timestamp.u64));
        frame.buf = buf;
        frame.size = buf_size;
        CHECK (codec->op->encode(codec, &pcm_frame, buf_size, &frame));
        frame.timestamp = pcm_frame.timestamp;
        PJ_LOG(6, (THIS_FILE, "encoded packet: sz=%d ts=%llu",
                frame.size/sizeof(pj_uint16_t), frame.timestamp.u64));
        CHECK(pjmedia_port_put_frame(markov_port, &frame));
        read_ts.u64 += play_file_port->info.samples_per_frame;
        total_bytes += frame.size;
    }
    pjmedia_port_destroy(play_file_port);
    pjmedia_port_destroy(markov_port);
    pjmedia_port_destroy(leaky_bucket_port);
    if (show_stats){
        em_plc_statistics stats;
        double sample_length = (double)read_ts.u64 / play_file_port->info.clock_rate;
        pjmedia_plc_port_get_statistics(plc_port, &stats);
        printf(
                "Emulation statistics\n"
                "          sample total length: %.2f seconds\n"
                "           total packets sent: %u\n"
                "                 packets lost: %u\n"
                "             packets received: %u\n"
                "           avg bits per frame: %u\n"
                "             expected avg bps: %u\n"
                "                 real avg bps: %.2f\n"
                "                 loss percent: %.2f\n",
            sample_length,
            stats.total, stats.lost, stats.received,
            total_bytes * 8 / stats.total,
            codec_param.info.avg_bps,
            total_bytes * 8 / sample_length,
            100.0 * stats.lost/stats.total);
    }
    pjmedia_port_destroy(plc_port);
    pjmedia_port_destroy(silence_port);
    pjmedia_port_destroy(rec_file_port);
}
