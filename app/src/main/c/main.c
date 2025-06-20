#include <dirent.h>
#include <jni.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "config.h"
#include "logging.h"
#include "properties.h"
#include "render.h"

static const char *FILENAME = "main.c";

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 128
#endif

static ANativeActivity *activity;

static void
path_concat (char *const dst, const char *const restrict prefix,
             const char *const restrict file)
{
    const size_t pfxlen     = strlen (prefix);
    const size_t flen       = strlen (file);
    const size_t malloc_siz = pfxlen + flen + 2;

    memcpy (dst, prefix, pfxlen);
    dst[pfxlen] = '/';
    memcpy (dst + pfxlen + 1, file, flen + 1);
    dst[malloc_siz - 1] = '\0';
}

static int
load_dir (const char *path)
{
    struct dirent *dir;
    DIR           *dp = opendir (path);

    if (dp == NULL) {
        logef ("opendir failed for path `%s': %s", path, strerror (errno));
        return -1;
    }

    while ((dir = readdir (dp)) != NULL) {
        if (dir->d_type != DT_REG) {
            logvf ("skipping file `%s': not a DT_REG file", dir->d_name);
        }

        // push into queue
    }
}

static void *
tfn_audio_play (void *errstat)
{
    pthread_mutex_lock (&render_mx);

    while (render_ready == false)
        pthread_cond_wait (&render_cv, &render_mx);

    pthread_mutex_unlock (&render_mx);

    logi ("audio_play thread recieved signal");

    const char *fn_in = "/sdcard/Download/audio.m4a";
    static char fn_out[MAX_PATH_LEN];
    path_concat (fn_out, activity->internalDataPath, NCAP_AUDIO_CACHE_FILE);

    logif ("converting `%s' to WAV file `%s'...", fn_in, fn_out);

    int *e = (int *)errstat;

    if ((*e = libav_cvt_cwav (fn_in, fn_out)) != NCAP_OK) {
        logef ("ERROR: libav_cvt_wav failed with code %d\n", *e);
        goto exit;
    }

    logi ("playing audio...");

    if ((*e = audio_play (fn_out)) != NCAP_OK) {
        logef ("ERROR: libav_cvt_wav failed with code %d\n", *e);
        goto exit;
    }

exit:
    // free (fn_out);
    pthread_exit (NULL);
}

int
main (void)
{
    activity = GetAndroidApp ()->activity;

    static char cfgfile[MAX_PATH_LEN];
    path_concat (cfgfile, activity->internalDataPath, NCAP_CONFIG_FILE);
    logdf ("initializing config file `%s'", cfgfile);
    switch (config_init (cfgfile)) {
        case CONFIG_INIT_CREAT:
            logi ("creating config...");
            ncap_config.aaudio_optimize = 2; // power saving
            ncap_config.cur_track       = 0;
            ncap_config.isrepeat        = false;
            ncap_config.isshuffle       = false;
            ncap_config.volume          = 100;
            config_write (ncap_config);
            break;
        case CONFIG_INIT_EXISTS:
            logi ("config exists. reading config...");
            config_read (ncap_config);
            break;
        case CONFIG_ERR:
        default:
            loge ("ERROR: config init failed");
    }

    config_logdump ();

    pthread_t audio_tid;
    int       stat;

    pthread_create (&audio_tid, NULL, tfn_audio_play, &stat);
    logi ("spawned audio_play thread");

    render ();

    logi ("joining threads...");
    pthread_join (audio_tid, NULL);
    logdf ("audio_play thread joined with a status code of %d...", stat);

    logi ("deinit config...");
    config_deinit ();
    logi ("main finished");
}
