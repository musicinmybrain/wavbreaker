/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include <gtk/gtk.h>
 
#include "wavbreaker.h"
#include "linuxaudio.h"
#include "sample.h"
#include "wav.h"
#include "cdda.h"

#define CDDA 1
#define WAV  2 

static SampleInfo sampleInfo;
static unsigned long sample_start = 0;
static int playing = 0;
static int audio_type;
static int audio_fd;

static char *audio_dev = "/dev/dsp";
static char *sample_file = NULL;
static FILE *sample_fp = NULL;

static pthread_t thread;
static pthread_attr_t thread_attr;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* typedef and struct stuff for new thread open junk */

typedef struct WriteThreadData_ WriteThreadData;
struct WriteThreadData_ {
	GList *tbl;
	WriteInfo *write_info;
};
	WriteThreadData wtd;

typedef struct OpenThreadData_ OpenThreadData;
struct OpenThreadData_ {
	GraphData *graphData;
	double *pct;
};
	OpenThreadData open_thread_data;

static void sample_max_min(GraphData *graphData, double *pct);

void sample_set_audio_dev(char *str)
{
	audio_dev = str;
}

char * sample_get_audio_dev()
{
	return audio_dev;
}

char * sample_get_sample_file()
{
	return sample_file;
}

gint sample_get_playing()
{
	return playing;
}

static void *
play_thread(void *thread_data)
{
	int ret, i;
	guint *play_marker = (guint *)thread_data;
	unsigned char devbuf[BUF_SIZE];

	if ((audio_fd = open_device(audio_dev, &sampleInfo)) < 0) {
		playing = 0;
		return NULL;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_fp, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_fp, devbuf, BUF_SIZE,
			sample_start + (BUF_SIZE * i++));
	}

	while (ret > 0 && ret <= BUF_SIZE) {
	        write(audio_fd, devbuf, ret);

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_fp, devbuf, BUF_SIZE,
				sample_start + (BUF_SIZE * i++));
		}

		*play_marker = ((BUF_SIZE * i) + sample_start) / CD_BLOCK_SIZE;
	}

	pthread_mutex_lock(&mutex);

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);

	return NULL;
}

int play_sample(gulong startpos, gulong *play_marker)
{       
	int ret;

	if (playing) {
		return 2;
	}

	if (sample_file == NULL) {
		return 3;
	}

	playing = 1;
	sample_start = startpos * BLOCK_SIZE;

	/* setup thread */

	if ((ret = pthread_mutex_init(&mutex, NULL)) != 0) {
		perror("Return from pthread_mutex_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_init(&thread_attr)) != 0) {
		perror("Return from pthread_attr_init");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_attr_setdetachstate(&thread_attr,
		PTHREAD_CREATE_DETACHED)) != 0) {

		perror("Return from pthread_attr_setdetachstate");
		printf("Error #%d\n", ret);
		return 1;
	}

	if ((ret = pthread_create(&thread, &thread_attr, play_thread,
			play_marker)) != 0) {

		perror("Return from pthread_create");
		printf("Error #%d\n", ret);
		return 1;
	}

	return 0;
}               

void stop_sample()
{       
	if (pthread_mutex_trylock(&mutex)) {
		return;
	}

	if (!playing) {
		pthread_mutex_unlock(&mutex);
		return;
	}

	if (pthread_cancel(thread)) {
		perror("Return from pthread_cancel");
	        printf("trouble cancelling the thread\n");
		pthread_mutex_unlock(&mutex);
		return;
	}

	close_device(audio_fd);
	playing = 0;

	pthread_mutex_unlock(&mutex);
}

static void *
open_thread(void *data)
{
	OpenThreadData *thread_data = data;

	sample_max_min(thread_data->graphData,
	               thread_data->pct);

	return NULL;
}

void sample_open_file(const char *filename, GraphData *graphData, double *pct)
{
	if (sample_file != NULL) {
		free(sample_file);
	}
	sample_file = strdup(filename);

	if (strstr(sample_file, ".wav")) {
		wav_read_header(sample_file, &sampleInfo);
		audio_type = WAV;
	} else {
		cdda_read_header(sample_file, &sampleInfo);
		audio_type = CDDA;
	}

	if ((sample_fp = fopen(sample_file, "r")) == NULL) {
		printf("error opening %s\n", sample_file);
		return;
	}

	open_thread_data.graphData = graphData;
	open_thread_data.pct = pct;

/* start new thread stuff */
	if (pthread_attr_init(&thread_attr) != 0) {
		perror("return from pthread_attr_init");
	}

	if (pthread_attr_setdetachstate(&thread_attr,
			PTHREAD_CREATE_DETACHED) != 0) {
		perror("return from pthread_attr_setdetachstate");
	}

	if (pthread_create(&thread, &thread_attr, open_thread, 
			   &open_thread_data) != 0) {
		perror("Return from pthread_create");
	}
/* end new thread stuff */
}

static void sample_max_min(GraphData *graphData, double *pct)
{
	int tmp, min, max;
	int i, k, ret;
	int numSampleBlocks;
	double tmp_sample_calc;
	unsigned char devbuf[BLOCK_SIZE];
	Points *graph_data;

	tmp_sample_calc = sampleInfo.numBytes / (sampleInfo.bitsPerSample / 8);
	tmp_sample_calc = tmp_sample_calc / BLOCK_SIZE;
	tmp_sample_calc = tmp_sample_calc * sampleInfo.channels;
	numSampleBlocks = (int) (tmp_sample_calc +  1);

	/* DEBUG CODE START */
	/*
	printf("\nsampleInfo.numBytes: %lu\n", sampleInfo.numBytes);
	printf("sampleInfo.bitsPerSample: %d\n", sampleInfo.bitsPerSample);
	printf("BLOCK_SIZE: %d\n", BLOCK_SIZE);
	printf("sampleInfo.channels: %d\n\n", sampleInfo.channels);
	*/
	/* DEBUG CODE END */

	graph_data = (Points *)malloc(numSampleBlocks * sizeof(Points));

	if (graph_data == NULL) {
		printf("NULL returned from malloc of graph_data\n");
		return;
	}

	i = 0;

	if (audio_type == CDDA) {
		ret = cdda_read_sample(sample_fp, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	} else if (audio_type == WAV) {
		ret = wav_read_sample(sample_fp, devbuf, BLOCK_SIZE,
			BLOCK_SIZE * i);
	}

	while (ret == BLOCK_SIZE) {
		min = max = 0;
		for (k = 0; k < ret; k++) {
			if (sampleInfo.bitsPerSample == 8) {
				tmp = devbuf[k];
				tmp -= 128;
			} else if (sampleInfo.bitsPerSample == 16) {
				tmp = (char)devbuf[k+1] << 8 | (char)devbuf[k];
				k++;
			}

			if (tmp > max) {
				max = tmp;
			} else if (tmp < min) {
				min = tmp;
			}
		}

		graph_data[i].min = min;
		graph_data[i].max = max;

		if (audio_type == CDDA) {
			ret = cdda_read_sample(sample_fp, devbuf, BLOCK_SIZE,
					BLOCK_SIZE * i);
		} else if (audio_type == WAV) {
			ret = wav_read_sample(sample_fp, devbuf, BLOCK_SIZE,
					BLOCK_SIZE * i);
		}

		*pct = (double) i / numSampleBlocks;
		i++;
	}

	*pct = 1.0;

	graphData->numSamples = numSampleBlocks;

	if (graphData->data != NULL) {
		free(graphData->data);
	}
	graphData->data = graph_data;

	if (sampleInfo.bitsPerSample == 8) {
		graphData->maxSampleValue = UCHAR_MAX;
	} else if (sampleInfo.bitsPerSample == 16) {
		graphData->maxSampleValue = SHRT_MAX;
	}

	/* DEBUG CODE START */
	/*
	printf("\ni: %d\n", i);
	printf("graphData->numSamples: %ld\n", graphData->numSamples);
	printf("graphData->maxSampleValue: %ld\n\n", graphData->maxSampleValue);
	*/
	/* DEBUG CODE END */
}

static void *
write_thread(void *data)
{
	WriteThreadData *thread_data = data;

	GList *tbl_head = thread_data->tbl;
	GList *tbl_cur, *tbl_next;
	TrackBreak *tb_cur, *tb_next;
	WriteInfo *write_info = thread_data->write_info;

	int i;
	int ret;
	int index;
	unsigned long start_pos, end_pos;
	char filename[1024];
	char str_tmp[1024];

	write_info->num_files = 0;
	write_info->cur_file = 0;
	write_info->sync = 0;

	i = 1;
	tbl_cur = tbl_head;
	while (tbl_cur != NULL) {
		index = g_list_position(tbl_head, tbl_cur);
		tb_cur = (TrackBreak *)g_list_nth_data(tbl_head, index);

		if (tb_cur->write == TRUE) {
			write_info->num_files++;
		}

		tbl_cur = g_list_next(tbl_cur);
	}

	i = 1;
	tbl_cur = tbl_head;
	tbl_next = g_list_next(tbl_cur);

	while (tbl_cur != NULL) {
		index = g_list_position(tbl_head, tbl_cur);
		tb_cur = (TrackBreak *)g_list_nth_data(tbl_head, index);
		if (tb_cur->write == TRUE) {
			start_pos = tb_cur->offset * BLOCK_SIZE;

			if (tbl_next == NULL) {
				end_pos = 0;
				tb_next = NULL;
			} else {
				index = g_list_position(tbl_head, tbl_next);
				tb_next = (TrackBreak *)g_list_nth_data(tbl_head, index);
				end_pos = tb_next->offset * BLOCK_SIZE;
			}

			/* add output directory to filename */
//			strcpy(str_tmp, "/data/tmp/wavbreaker");
			strcpy(filename, tb_cur->filename);

			/* add file number to filename */
			/* !!now doing this in the track break list!!
			if (i < 10) {
				sprintf(str_tmp, "0%d", i);
			} else {
				sprintf(str_tmp, "%d", i);
			}
			strcat(filename, str_tmp);
			*/

			/* add file extension to filename */
			if ((audio_type == WAV) && (!strstr(filename, ".wav"))) {
				strcat(filename, ".wav");
			} else if ((audio_type == CDDA) && (!strstr(filename, ".dat"))) {
				strcat(filename, ".dat");
			}
			write_info->cur_file++;
			if (write_info->cur_filename != NULL) {
				free(write_info->cur_filename);
			}
			write_info->cur_filename = strdup(filename);

			if (audio_type == CDDA) {
				ret = cdda_write_file(sample_fp, filename, BUF_SIZE,
							start_pos, end_pos);
			} else if (audio_type == WAV) {
				ret = wav_write_file(sample_fp, filename, BLOCK_SIZE,
							&sampleInfo, start_pos, end_pos,
							&write_info->pct_done);
			}
			i++;
		}

		tbl_cur = g_list_next(tbl_cur);
		tbl_next = g_list_next(tbl_next);
	}
	write_info->sync = 1;

	return NULL;
}

void sample_write_files(const char *filename, GList *tbl, WriteInfo *write_info)
{
//	WriteThreadData wtd;

	wtd.tbl = tbl;
	wtd.write_info = write_info;

	if (sample_file != NULL) {
		free(sample_file);
	}
	sample_file = strdup(filename);

	if ((sample_fp = fopen(sample_file, "r")) == NULL) {
		printf("error opening %s\n", sample_file);
		return;
	}

/* start new thread stuff */
	if (pthread_attr_init(&thread_attr) != 0) {
		perror("return from pthread_attr_init");
	}

	if (pthread_attr_setdetachstate(&thread_attr,
			PTHREAD_CREATE_DETACHED) != 0) {
		perror("return from pthread_attr_setdetachstate");
	}

	if (pthread_create(&thread, &thread_attr, write_thread, &wtd) != 0) {
		perror("Return from pthread_create");
	}
/* end new thread stuff */
}
