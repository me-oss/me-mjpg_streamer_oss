/*******************************************************************************
 #                                                                              #
 #      MJPG-streamer allows to stream JPG frames from an input-plugin          #
 #      to several output plugins                                               #
 #                                                                              #
 #      Copyright (C) 2007 busybox-project (base64 function)                    #
 #      Copyright (C) 2007 Tom Stöveken                                         #
 #                                                                              #
 # This program is free software; you can redistribute it and/or modify         #
 # it under the terms of the GNU General Public License as published by         #
 # the Free Software Foundation; version 2 of the License.                      #
 #                                                                              #
 # This program is distributed in the hope that it will be useful,              #
 # but WITHOUT ANY WARRANTY; without even the implied warranty of               #
 # MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
 # GNU General Public License for more details.                                 #
 #                                                                              #
 # You should have received a copy of the GNU General Public License            #
 # along with this program; if not, write to the Free Software                  #
 # Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
 #                                                                              #
 *******************************************************************************/
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <dlfcn.h>

#include "../../mjpg_streamer.h"
#include "../../utils.h"
#include "../input_mlsicam/mlsErrors.h"
#include "../input_mlsicam/mlsAuthen.h"
#include "../input_mlsicam/mlsOTA.h"
#include "../input_mlsicam/postoffice.h"
#include "httpd.h"
#include "url_encode.h"
#include "../../debug.h"
#ifdef DMALLOC
#include "dmalloc.h"
#endif


static globals *pglobal;
extern context servers[MAX_OUTPUT_PLUGINS];
//#define MS_LIMIT_CLIENT;//mls@dev03 20110725 limit 1 client
#if defined (MS_LIMIT_CLIENT)
unsigned char nbClient = 0;
#endif
static int output_delay_ms = 0;
#define MAX_THREAD_COUNT	8
#define MAX_APPLETVA_STREAM_COUNT 4
static int total_thread_count = 0;
static int total_appletva_count = 0;



int adhocmode =0;
int (*GenerateSessionKey)(char*, struct sockaddr_in*, char*);
int (*ResetCreateSessionKeyProcess)();
int (*ReadMasterKey)(MasterKey_st *);
int (*CheckIPisLocalorRemoteorServer)(struct sockaddr_in, struct sockaddr_in,
		struct sockaddr_in);
int (*TimerCheck)();
void (*ClearRemoteIP)();
void (*SetRemoteIP)(char* ip);
char* (*RetrieveSessionKey)();

/**
 *  PostOffice function pointers
 *
 */
int (*PostOffice_ReceiveMail_fp)(char** pdata, int* pdatalen,int MailboxID, struct timeval* ts);
int (*PostOffice_MarkMailRead_fp)(int MailboxID);
int (*PostOffice_Register_fp)(); // return MailboxID
int (*PostOffice_DeRegister_fp)(int MailboxID);
int (*PostOffice_AvailableTotalMail_fp)(int MailboxID, int* totallen, int* totalmail);


extern int mlsIcam_IsinUAPMode();
extern int mlsIcam_IsVideoTestFinish();

int (*mlsIcam_IsinUAPMode_fp)();
int (*mlsIcam_IsVideoTestFinish_fp)();
/******************************************************************************
 Description.: initializes the iobuffer structure properly
 Input Value.: pointer to already allocated iobuffer
 Return Value: iobuf
 ******************************************************************************/
void init_iobuffer(iobuffer *iobuf) {
	memset(iobuf->buffer, 0, sizeof(iobuf->buffer));
	iobuf->level = 0;
}

/******************************************************************************
 Description.: initializes the request structure properly
 Input Value.: pointer to already allocated req
 Return Value: req
 ******************************************************************************/
void init_request(request *req) {
	req->type = A_UNKNOWN;
	req->parameter = NULL;
	req->client = NULL;
	req->credentials = NULL;
}

/******************************************************************************
 Description.: If strings were assigned to the different members free them
 This will fail if strings are static, so always use strdup().
 Input Value.: req: pointer to request structure
 Return Value: -
 ******************************************************************************/
void free_request(request *req) {
	if (req->parameter != NULL)
		free(req->parameter);
	if (req->client != NULL)
		free(req->client);
	if (req->credentials != NULL)
		free(req->credentials);
}

/******************************************************************************
 Description.: read with timeout, implemented without using signals
 tries to read len bytes and returns if enough bytes were read
 or the timeout was triggered. In case of timeout the return
 value may differ from the requested bytes "len".
 Input Value.: * fd.....: fildescriptor to read from
 * iobuf..: iobuffer that allows to use this functions from multiple
 threads because the complete context is the iobuffer.
 * buffer.: The buffer to store values at, will be set to zero
 before storing values.
 * len....: the length of buffer
 * timeout: seconds to wait for an answer
 Return Value: * buffer.: will become filled with bytes read
 * iobuf..: May get altered to save the context for future calls.
 * func().: bytes copied to buffer or -1 in case of error
 ******************************************************************************/
int _read(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout) {
	int copied = 0, rc, i;
	fd_set fds;
	struct timeval tv;

	memset(buffer, 0, len);

	while ((copied < len)) {
		i = MIN(iobuf->level, len - copied);
		memcpy(buffer + copied, iobuf->buffer + IO_BUFFER - iobuf->level, i);

		iobuf->level -= i;
		copied += i;
		if (copied >= len)
			return copied;

		/* select will return in case of timeout or new data arrived */
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if ((rc = select(fd + 1, &fds, NULL, NULL, &tv)) <= 0) {
			if (rc < 0)
				exit(EXIT_FAILURE);

			/* this must be a timeout */
			return copied;
		}

		init_iobuffer(iobuf);

		/*
		 * there should be at least one byte, because select signalled it.
		 * But: It may happen (very seldomly), that the socket gets closed remotly between
		 * the select() and the following read. That is the reason for not relying
		 * on reading at least one byte.
		 */
		if ((iobuf->level = read(fd, &iobuf->buffer, IO_BUFFER)) <= 0) {
			/* an error occured */
			return -1;
		}

		/* align data to the end of the buffer if less than IO_BUFFER bytes were read */
		memmove(iobuf->buffer + (IO_BUFFER - iobuf->level), iobuf->buffer,
				iobuf->level);
	}

	return 0;
}

/******************************************************************************
 Description.: Read a single line from the provided fildescriptor.
 This funtion will return under two conditions:
 * line end was reached
 * timeout occured
 Input Value.: * fd.....: fildescriptor to read from
 * iobuf..: iobuffer that allows to use this functions from multiple
 threads because the complete context is the iobuffer.
 * buffer.: The buffer to store values at, will be set to zero
 before storing values.
 * len....: the length of buffer
 * timeout: seconds to wait for an answer
 Return Value: * buffer.: will become filled with bytes read
 * iobuf..: May get altered to save the context for future calls.
 * func().: bytes copied to buffer or -1 in case of error
 ******************************************************************************/
/* read just a single line or timeout */
int _readline(int fd, iobuffer *iobuf, void *buffer, size_t len, int timeout) {
	char c = '\0', *out = buffer;
	int i;

	memset(buffer, 0, len);

	for (i = 0; i < len && c != '\n'; i++) {
		if (_read(fd, iobuf, &c, 1, timeout) <= 0) {
			/* timeout or error occured */
			return -1;
		}
		*out++ = c;
	}

	return i;
}

/******************************************************************************
 Description.: Decodes the data and stores the result to the same buffer.
 The buffer will be large enough, because base64 requires more
 space then plain text.
 Hints.......: taken from busybox, but it is GPL code
 Input Value.: base64 encoded data
 Return Value: plain decoded data
 ******************************************************************************/
void decodeBase64(char *data) {
	const unsigned char *in = (const unsigned char *) data;
	/* The decoded size will be at most 3/4 the size of the encoded */
	unsigned ch = 0;
	int i = 0;

	while (*in) {
		int t = *in++;

		if (t >= '0' && t <= '9')
			t = t - '0' + 52;
		else if (t >= 'A' && t <= 'Z')
			t = t - 'A';
		else if (t >= 'a' && t <= 'z')
			t = t - 'a' + 26;
		else if (t == '+')
			t = 62;
		else if (t == '/')
			t = 63;
		else if (t == '=')
			t = 0;
		else
			continue;

		ch = (ch << 6) | t;
		i++;
		if (i == 4) {
			*data++ = (char) (ch >> 16);
			*data++ = (char) (ch >> 8);
			*data++ = (char) ch;
			i = 0;
		}
	}
	*data = '\0';
}

/******************************************************************************
 Description.: Send a complete HTTP response and a single JPG-frame.
 Input Value.: fildescriptor fd to send the answer to
 Return Value: -
 ******************************************************************************/
void send_snapshot(int fd) {
	unsigned char *frame = NULL;
	int frame_size = 0;
	char buffer[BUFFER_SIZE] = { 0 };

	/* wait for a fresh frame */
	pthread_cond_wait(&pglobal->db_update, &pglobal->db);

	/* read buffer */
	frame_size = pglobal->size;

	/* allocate a buffer for this single frame */
	if ((frame = malloc(frame_size + 1)) == NULL) {
		//free(frame);
		pthread_mutex_unlock(&pglobal->db);
		send_error(fd, 500, "not enough memory");
		return;
	}

	memcpy(frame, pglobal->buf, frame_size);
	DBG("got frame (size: %d kB)\n", frame_size / 1024);

	pthread_mutex_unlock(&pglobal->db);

	//mls@dev03 110131 debug info
//  printf("sending SNAPSHOT now...\n");

	/* write the response */
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			STD_HEADER
			"Content-type: image/jpeg\r\n"
			"\r\n");

	/* send header and image now */
	if (write(fd, buffer, strlen(buffer)) < 0) {
		free(frame);
		return;
	}
	write(fd, frame, frame_size);

	free(frame);

	//mls@dev03 20110222 increase number request of client
	pthread_mutex_lock(&(pglobal->blinkled_mutex));
	pglobal->countRequestClient++;
	pthread_mutex_unlock(&(pglobal->blinkled_mutex));
}

/******************************************************************************
 Description.: Send a complete HTTP response and a stream of JPG-frames.
 Input Value.: fildescriptor fd to send the answer to
 Return Value: -
 ******************************************************************************/
void send_stream(int fd, int isRemote) {
	unsigned char *frame = NULL, *tmp = NULL;
	int frame_size = 0, max_frame_size = 0;
	char buffer[BUFFER_SIZE] = { 0 };

	DBG("preparing header\n");

	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			STD_HEADER
			"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n"
			"\r\n"
			"--" BOUNDARY "\r\n");

	if (write(fd, buffer, strlen(buffer)) < 0) {
		goto CLEANUP;
	}

	DBG("Headers send, sending stream now\n");

	//mls@dev03 110131 debug info
	printf("sending stream STREAM now...\n");

	while (!pglobal->stop) {

		/* wait for fresh frames */
		pthread_mutex_lock(&pglobal->db);
		pthread_cond_wait(&pglobal->db_update, &pglobal->db);

		if (isRemote == 1) {
			if (pglobal->RemoteIPFlag == 0) {
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				pglobal->RemoteIPFlag = 1;
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			}
			if (TimerCheck() == 0) // timeout
					{
				printf("Timeout 2\n");
				send_error(fd, HTTP_AUTHEN_ERROR_TIMEOUT, "Timeout2");
				pthread_mutex_unlock(&pglobal->db);
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				memset(pglobal->RandomKey, 0, 9);
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
				goto CLEANUP;
			}
		}
		//printf("isRemote %d\n",isRemote);
		/* read buffer */
		frame_size = pglobal->size;

		/* check if framebuffer is large enough, increase it if necessary */
		if (frame_size > max_frame_size) {
			printf("increasing buffer size to %d\n", frame_size);

			max_frame_size = frame_size + TEN_K;
			if ((tmp = realloc(frame, max_frame_size)) == NULL) {
				//free(frame);
				pthread_mutex_unlock(&pglobal->db);
				send_error(fd, 500, "not enough memory");
				goto CLEANUP;
			}

			frame = tmp;
		}

		memcpy(frame, pglobal->buf, frame_size);
		DBG("got frame (size: %d kB)\n", frame_size / 1024);

		pthread_mutex_unlock(&pglobal->db);

		/*
		 * print the individual mimetype and the length
		 * sending the content-length fixes random stream disruption observed
		 * with firefox
		 */
		sprintf(buffer, "Content-Type: image/jpeg\r\n"
				"Content-Length: %d\r\n"
				"\r\n", frame_size);
		DBG("sending intemdiate header\n");
		if (write(fd, buffer, strlen(buffer)) < 0)
			break;

		DBG("sending frame\n");
		if (write(fd, frame, frame_size) < 0)
			break;

		DBG("sending boundary\n");
		sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
		if (write(fd, buffer, strlen(buffer)) < 0)
			break;

		//mls@dev03 20110222 increase number request of client
		pthread_mutex_lock(&(pglobal->blinkled_mutex));
		pglobal->countRequestClient++;
		pthread_mutex_unlock(&(pglobal->blinkled_mutex));

	}

	CLEANUP: 
	if (frame)
	    free(frame);
	if (isRemote == 1) {
		if (pglobal->RemoteIPFlag > 0) {
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			pglobal->RemoteIPFlag = 0;
			memset(&pglobal->RemoteIP, 0, sizeof(pglobal->RemoteIP));
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			printf("Freeing the resource now1\n");
		}
	}
}

void intToByteArray(int iNb, char *abNb) {
	char i, offset;

	for (i = 0; i < 4; i++) {
		offset = (4 - 1 - i) * 8;
		abNb[i] = (iNb >> offset) & 0xFF;
	}
}
/*
 * mls@dev03 10929 send stream applet to show video and audio
 */
#define MAX_FRAMESIZE 4096
//#define AUDIO_DEBUG

void send_applet_vastream(int fd, int isRemote) {
	unsigned char *frame = NULL, *tmp = NULL;
	int frame_size = 0, max_frame_size = 0;
	char buffer[BUFFER_SIZE] = { 0 };
	int iret, icamLenHeader;
	mlsIcamHeaderVA_st icamHeaderVA;
	int totalsend = 0, bytesend = 0;
	int audiosize,mailbox_size;
	unsigned char* audiobuf;
	int mailbox_id = -1;

#ifdef AUDIO_DEBUG
	FILE* fp;
#endif
	DBG("preparing header\n");
	total_appletva_count++;
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			STD_HEADER
			"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n"
			"\r\n"
			"--" BOUNDARY "\r\n");

	if (write(fd, buffer, strlen(buffer)) < 0) {
		goto exit;
	}

	DBG("Headers send, sending stream now\n");
#ifdef AUDIO_DEBUG
	fp = fopen("/mlsrb/mlswwwn/data.pcm","wb+");
#endif

	//mls@dev03 110131 debug info
	printf("sending stream JAVA APPLET VIDEO + AUDIO now...\n");


	//mls@dev03 101018 reset global audio buffer
//	*paudiosize = 0;
	pglobal->countOFBuffAudio = 0;
    //! Register Postoffice
    mailbox_id = PostOffice_Register_fp();
    char * postoffice_audiobuf = NULL;
    int mail_size = 0;
	while (!pglobal->stop) {
		/* wait for fresh frames */
		pthread_mutex_lock(&pglobal->db);
		iret = pthread_cond_wait(&pglobal->db_update, &pglobal->db);
        //! Receive mail
        postoffice_audiobuf = NULL;
        PostOffice_AvailableTotalMail_fp(mailbox_id,&audiosize,&mailbox_size);

        //printf("Finished reading mail %d\n",*paudiosize);
		if (iret != 0) {
			printf("wait error %d \n", iret);
		}
		if (isRemote==1) {
			if (pglobal->RemoteIPFlag == 0) {
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				pglobal->RemoteIPFlag = 1;
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			}
/*
			if (TimerCheck() == 0) // timeout
					{
				printf("Timeout 2\n");
				send_error(fd, HTTP_AUTHEN_ERROR_TIMEOUT, "Timeout2");
				pthread_mutex_unlock(&pglobal->db);
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				memset(pglobal->RandomKey, 0, 9);
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
				goto exit;
			}
*/
		}
		/* read buffer */
		frame_size = pglobal->size + audiosize + sizeof(icamHeaderVA);

		/* check if framebuffer is large enough, increase it if necessary */
		if (frame_size > max_frame_size) {
			printf("increasing buffer size to %d\n", frame_size);

			max_frame_size = frame_size + TEN_K;
			if ((tmp = realloc(frame, max_frame_size)) == NULL) {
				pthread_mutex_unlock(&pglobal->db);
				send_error(fd, 500, "not enough memory");
				printf("applet stream not enough memory\n");
				goto exit;
			}

			frame = tmp;
		}

		/* mls@dev03 fill data to frame */
		icamLenHeader = sizeof(icamHeaderVA);
		//printf("icamlenHeader = %d\n",icamLenHeader);
		memset((char*) &icamHeaderVA, 0, icamLenHeader);

		icamHeaderVA.flagReset = pglobal->flagReset;
		if (icamHeaderVA.flagReset) {
			printf("out flag reset \n");
		}
		//icamHeaderVA.flagReset |= 0x02 ; // mentioned this is PCM data

		if (audiosize > 0) {
			intToByteArray(audiosize, icamHeaderVA.abLenAudio);
			icamHeaderVA.countOFbuffAudio = 0;
		}

		intToByteArray(pglobal->indexCurrentJPEG,
				icamHeaderVA.abIndexCurrentJpgFrame);

		//mls@dev03 add JPEG resolution to header
		icamHeaderVA.resolution = pglobal->resolutionValue;

		//mls@dev03 20111103 temperature value
		intToByteArray(pglobal->tempValue, icamHeaderVA.abTempValue);
		
		/* copy icam header to buffer frame to send*/
		memcpy(frame, (char*) &icamHeaderVA, icamLenHeader);
		//printf("Audio total %d bytes in %d mailbox\n",audiosize,mailbox_size);
		//audio data
		if (audiosize > 0) {
			int i = 0;
			int tmpsize = 0;
			int err;
			struct timeval ts;
			for (i=0;i<mailbox_size;i++)
			{
				err = PostOffice_ReceiveMail_fp(&postoffice_audiobuf, &mail_size, mailbox_id,&ts);
				if (err == 0)
				{
					memcpy(frame+icamLenHeader+tmpsize,postoffice_audiobuf,mail_size);
#ifdef AUDIO_DEBUG
					fwrite(postoffice_audiobuf,1,mail_size,fp);
#endif
					//printf("Retrieve Mail with delay %d sec.%d usec\n",ts.tv_sec,ts.tv_usec);
				}
				else
				{
					printf("Receivemail error : %d\n",err);
				}
				PostOffice_MarkMailRead_fp(mailbox_id);
				tmpsize += mail_size;
			}
			//printf("Tmpsize vs Totalsize (%d vs %d)\n",tmpsize,audiosize);
		}

		//JPEG data
		memcpy(frame + icamLenHeader + audiosize, pglobal->buf,
				pglobal->size);

		DBG("got frame (size: %d kB)\n", frame_size / 1024);

		pglobal->countOFBuffAudio = 0;

//		//mls@dev03 20110222 increase number request of client
//		pthread_mutex_lock( &(pglobal->blinkled_mutex) );
//		pglobal->countRequestClient++;
//		pthread_mutex_unlock( &(pglobal->blinkled_mutex) );

		pthread_mutex_unlock(&pglobal->db);

		/*
		 * print the individual mimetype and the length
		 * sending the content-length fixes random stream disruption observed
		 * with firefox
		 */
		sprintf(buffer, "Content-Type: image/jpeg\r\n"
				"Content-Length: %d\r\n"
				"\r\n", frame_size);
		DBG("sending intemdiate header\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			printf("********write http header error\n");
			goto exit;
		}

		DBG("sending frame\n");
//	    printf("sendapplet send frame\n");

#if 0
		if( write(fd, frame, frame_size) < 0 )
		{
//	    	break;
			printf("********write http frame error\n");
			goto exit;
		}
#else

		totalsend = 0;
		while (totalsend < frame_size) {
			int bytewrite;
			bytesend = (frame_size - totalsend);
			bytesend = (bytesend > MAX_FRAMESIZE) ? MAX_FRAMESIZE : bytesend;
			bytewrite = write(fd, frame + totalsend, bytesend);
			if (bytewrite < 0) {
				printf("********write http frame error\n");
				goto exit;
			}
			//printf("Write %d bytes - %d\n",bytewrite,bytesend);
			totalsend += bytewrite;
		}
#endif

		DBG("sending boundary\n");
//	    printf("sendapplet send boundary\n");
		sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			printf("********write http boundary error\n");
			goto exit;
		}

		//mls@dev03 20110222 increase number request of client
		pthread_mutex_lock(&(pglobal->blinkled_mutex));
		pglobal->countRequestClient++;
		pthread_mutex_unlock(&(pglobal->blinkled_mutex));
		if (output_delay_ms != 0)
		{
		    if(isRemote == 2) // due for STUN MODE
		    {
			//printf("Sleep %d\n",output_delay_ms);
			usleep(output_delay_ms * 1000);
		    }
		}
	}

	exit:
#ifdef AUDIO_DEBUG
	fclose(fp);
#endif
    total_appletva_count --;
    if (mailbox_id != -1)
    {
	//! Deregister PostOffice
	if(PostOffice_DeRegister_fp(mailbox_id) != 0)
	{
    	    printf("Error: Deregister postoffice\n");
	}
    }
	printf("Going to exit the va thread\n");
	if (frame)
	    free(frame);
	if (isRemote == 1) {
		if (pglobal->RemoteIPFlag > 0) {
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			pglobal->RemoteIPFlag = 0;
			memset(&pglobal->RemoteIP, 0, sizeof(pglobal->RemoteIP));
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			printf("Freeing the resource2\n");
		}
	}
}


void send_applet_astream(int fd, int isRemote) {
	unsigned char *frame = NULL, *tmp = NULL;
	int frame_size = 0, max_frame_size = 0;
	char buffer[BUFFER_SIZE] = { 0 };
	int iret, icamLenHeader;
	mlsIcamHeaderVA_st icamHeaderVA;
	int totalsend = 0, bytesend = 0;
	int* paudiosize;
	unsigned char* audiobuf;
	int mailbox_id = -1;
	DBG("preparing header\n");

	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			STD_HEADER
			"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n"
			"\r\n"
			"--" BOUNDARY "\r\n");

	if (write(fd, buffer, strlen(buffer)) < 0) {
		goto exit;
	}

	DBG("Headers send, sending stream now\n");

	//mls@dev03 110131 debug info
	printf("sending stream JAVA APPLET VIDEO + AUDIO now...\n");

	if (isRemote == 1)
	{
		paudiosize = &pglobal->remotesizeAudio;
		audiobuf = pglobal->remotebufAudio;
	}
	else
	{
		paudiosize = &pglobal->sizeAudio;
		audiobuf = pglobal->bufAudio;
	}
	//mls@dev03 101018 reset global audio buffer
	*paudiosize = 0;
	pglobal->countOFBuffAudio = 0;
    //! Register Postoffice
    mailbox_id = PostOffice_Register_fp();
    char * postoffice_audiobuf = NULL;
    int mail_size = 0;
	while (!pglobal->stop) {
		pthread_mutex_lock(&pglobal->db);
		/* wait for fresh frames */
		iret = pthread_cond_wait(&pglobal->db_update, &pglobal->db);
        //! Receive mail
        postoffice_audiobuf = NULL;
        *paudiosize = 0;
        while(PostOffice_ReceiveMail_fp(&postoffice_audiobuf, &mail_size, mailbox_id,NULL) == 0)
        {
            if(*paudiosize + mail_size > GLOBAL_AUDIOBUFFER_SIZE)
            {
                printf("Overlow audiobuf\n");
                break;
            }else if(postoffice_audiobuf)
            {
                //printf("Copy to audiobuf %d\n",mail_size);
                memcpy(audiobuf + *paudiosize,postoffice_audiobuf,mail_size);
                if(PostOffice_MarkMailRead_fp(mailbox_id) != 0)
                    printf("Error: mark\n");
                *paudiosize += mail_size;
            }
        }
        //printf("Finished reading mail %d\n",*paudiosize);
		if (iret != 0) {
			printf("wait error %d \n", iret);
		}
		if (isRemote == 1) {
			if (pglobal->RemoteIPFlag == 0) {
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				pglobal->RemoteIPFlag = 1;
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			}
/*
			if (TimerCheck() == 0) // timeout
					{
				printf("Timeout 2\n");
				send_error(fd, HTTP_AUTHEN_ERROR_TIMEOUT, "Timeout2");
				pthread_mutex_unlock(&pglobal->db);
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				memset(pglobal->RandomKey, 0, 9);
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
				goto exit;
			}
*/
		}
		/* read buffer */
		frame_size = 0 + *paudiosize + sizeof(icamHeaderVA);

		/* check if framebuffer is large enough, increase it if necessary */
		if (frame_size > max_frame_size) {
			printf("increasing buffer size to %d\n", frame_size);

			max_frame_size = frame_size + TEN_K;
			if ((tmp = realloc(frame, max_frame_size)) == NULL) {
				pthread_mutex_unlock(&pglobal->db);
				send_error(fd, 500, "not enough memory");
				printf("applet stream not enough memory\n");
				goto exit;
			}

			frame = tmp;
		}

		/* mls@dev03 fill data to frame */
		icamLenHeader = sizeof(icamHeaderVA);
		memset((char*) &icamHeaderVA, 0, icamLenHeader);

		icamHeaderVA.flagReset = pglobal->flagReset;
		if (icamHeaderVA.flagReset) {
			printf("out flag reset \n");
		}
		//icamHeaderVA.flagReset |= 0x02 ; // mentioned this is PCM data
		if (pglobal->sizeAudio > 0) {
			intToByteArray(*paudiosize, icamHeaderVA.abLenAudio);
			icamHeaderVA.countOFbuffAudio = pglobal->countOFBuffAudio;
		}

		intToByteArray(pglobal->indexCurrentJPEG,
				icamHeaderVA.abIndexCurrentJpgFrame);

		//mls@dev03 add JPEG resolution to header
		icamHeaderVA.resolution = pglobal->resolutionValue;

		//mls@dev03 20111103 temperature value
		intToByteArray(pglobal->tempValue, icamHeaderVA.abTempValue);
		
		/* copy icam header to buffer frame to send*/
		memcpy(frame, (char*) &icamHeaderVA, icamLenHeader);

		//audio data
		if (*paudiosize > 0) {
			memcpy(frame + icamLenHeader, audiobuf,
					*paudiosize);
		}

		//JPEG data
		//memcpy(frame + icamLenHeader + pglobal->sizeAudio, pglobal->buf,
			//	pglobal->size);

		DBG("got frame (size: %d kB)\n", frame_size / 1024);

		//mls@dev03 101017 reset global audio buffer
		*paudiosize = 0;
		pglobal->countOFBuffAudio = 0;

//		//mls@dev03 20110222 increase number request of client
//		pthread_mutex_lock( &(pglobal->blinkled_mutex) );
//		pglobal->countRequestClient++;
//		pthread_mutex_unlock( &(pglobal->blinkled_mutex) );

		pthread_mutex_unlock(&pglobal->db);

		/*
		 * print the individual mimetype and the length
		 * sending the content-length fixes random stream disruption observed
		 * with firefox
		 */
		sprintf(buffer, "Content-Type: image/jpeg\r\n"
				"Content-Length: %d\r\n"
				"\r\n", frame_size);
		DBG("sending intemdiate header\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			printf("********write http header error\n");
			goto exit;
		}

		DBG("sending frame\n");
//	    printf("sendapplet send frame\n");

#if 0
		if( write(fd, frame, frame_size) < 0 )
		{
//	    	break;
			printf("********write http frame error\n");
			goto exit;
		}
#else

		totalsend = 0;
		while (totalsend < frame_size) {
			int bytewrite;
			bytesend = (frame_size - totalsend);
			bytesend = (bytesend > MAX_FRAMESIZE) ? MAX_FRAMESIZE : bytesend;
			bytewrite = write(fd, frame + totalsend, bytesend);
			if (bytewrite < 0) {
				printf("********write http frame error\n");
				goto exit;
			}
			//printf("Write %d bytes - %d\n",bytewrite,bytesend);
			totalsend += bytewrite;
		}
#endif

		DBG("sending boundary\n");
//	    printf("sendapplet send boundary\n");
		sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			printf("********write http boundary error\n");
			goto exit;
		}

		//mls@dev03 20110222 increase number request of client
		pthread_mutex_lock(&(pglobal->blinkled_mutex));
		pglobal->countRequestClient++;
		pthread_mutex_unlock(&(pglobal->blinkled_mutex));
	}

	exit:
    if (mailbox_id != -1)
    {
	//! Deregister PostOffice
	if(PostOffice_DeRegister_fp(mailbox_id) != 0)
	{
    	    printf("Error: Deregister postoffice\n");
	}
    }
	printf("Going to exit the va thread\n");
	if (frame)
	    free(frame);
	if (isRemote == 1) {
		if (pglobal->RemoteIPFlag > 0) {
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			pglobal->RemoteIPFlag = 0;
			memset(&pglobal->RemoteIP, 0, sizeof(pglobal->RemoteIP));
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			printf("Freeing the resource2\n");
		}
	}
}

/*
 * mls@dev03 101013 add send video stream in java applet
 */
void send_applet_vstream(int fd, int isRemote) {
	unsigned char *frame = NULL, *tmp = NULL;
	int frame_size = 0, max_frame_size = 0;
	char buffer[BUFFER_SIZE] = { 0 };
	int iret, icamLenHeader;
	mlsIcamHeaderV_st icamHeaderV;

	DBG("preparing header\n");

	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			STD_HEADER
			"Content-Type: multipart/x-mixed-replace;boundary=" BOUNDARY "\r\n"
			"\r\n"
			"--" BOUNDARY "\r\n");

	if (write(fd, buffer, strlen(buffer)) < 0) {
		free(frame);
		return;
	}

	DBG("Headers send, sending stream now\n");

	//mls@dev03 110131 debug info
	printf("sending stream JAVA APPLET VIDEO now...\n");

	icamLenHeader = sizeof(icamHeaderV);

	while (!pglobal->stop) {
		pthread_mutex_lock(&pglobal->db);
		/* wait for fresh frames */
		iret = pthread_cond_wait(&pglobal->db_update, &pglobal->db);

		if (iret != 0) {
			printf("wait error %d \n", iret);
		}
		if (isRemote == 1) {
			if (pglobal->RemoteIPFlag == 0) {
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				pglobal->RemoteIPFlag = 1;
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			}
/*
			if (TimerCheck() == 0) // timeout
					{
				printf("Timeout 2\n");
				send_error(fd, HTTP_AUTHEN_ERROR_TIMEOUT, "Timeout2");
				pthread_mutex_unlock(&pglobal->db);
				pthread_mutex_lock(&(pglobal->blinkled_mutex));
				memset(pglobal->RandomKey, 0, 9);
				pthread_mutex_unlock(&(pglobal->blinkled_mutex));
				goto exit;
			}
*/
		}
		/* read buffer */
		frame_size = pglobal->size + pglobal->sizeAudio + icamLenHeader;

		/* check if framebuffer is large enough, increase it if necessary */
		if (frame_size > max_frame_size) {
			printf("increasing buffer size to %d\n", frame_size);

			max_frame_size = frame_size + TEN_K;
			if ((tmp = realloc(frame, max_frame_size)) == NULL) {

				pthread_mutex_unlock(&pglobal->db);
				send_error(fd, 500, "not enough memory");
				printf("applet stream not enough memory\n");
				goto exit;
			}

			frame = tmp;
		}

		/* mls@dev03 fill data to frame */
		memset((char*) &icamHeaderV, 0, icamLenHeader);

		icamHeaderV.resolutionJpg = pglobal->resolutionValue;

		/* copy icam header to buffer frame to send*/
		memcpy(frame, (char*) &icamHeaderV, icamLenHeader);

		memcpy(frame + icamLenHeader, pglobal->buf, pglobal->size);

		pthread_mutex_unlock(&pglobal->db);

		/*
		 * print the individual mimetype and the length
		 * sending the content-length fixes random stream disruption observed
		 * with firefox
		 */
		sprintf(buffer, "Content-Type: image/jpeg\r\n"
				"Content-Length: %d\r\n"
				"\r\n", frame_size);
		DBG("sending intemdiate header\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			goto exit;
		}

		DBG("sending frame\n");
		if (write(fd, frame, frame_size) < 0) {
//	    	break;
			printf("********write http frame error\n");
			goto exit;
		}

		DBG("sending boundary\n");
		sprintf(buffer, "\r\n--" BOUNDARY "\r\n");
		if (write(fd, buffer, strlen(buffer)) < 0) {
//	    	break;
			printf("********write http boundary error\n");
			goto exit;
		}

		//mls@dev03 20110222 increase number request of client
		pthread_mutex_lock(&(pglobal->blinkled_mutex));
		pglobal->countRequestClient++;
		pthread_mutex_unlock(&(pglobal->blinkled_mutex));

	}

	exit: free(frame);
	if (isRemote == 1) {
		if (pglobal->RemoteIPFlag > 0) {
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			pglobal->RemoteIPFlag = 0;
			memset(&pglobal->RemoteIP, 0, sizeof(pglobal->RemoteIP));
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
		}
	}

}

/******************************************************************************
 Description.: Send error messages and headers.
 Input Value.: * fd.....: is the filedescriptor to send the message to
 * which..: HTTP error code, most popular is 404
 * message: append this string to the displayed response
 Return Value: -
 ******************************************************************************/
void send_error(int fd, int which, char *message) {
	char buffer[BUFFER_SIZE] = { 0 };

	if (which == 401) {
sprintf	(buffer, "HTTP/1.0 401 Unauthorized\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"WWW-Authenticate: Basic realm=\"MJPG-Streamer\"\r\n"
			"\r\n"
			"401: Not Authenticated!\r\n"
			"%s", message);
} else if ( which == 404 ) {
	sprintf(buffer, "HTTP/1.0 404 Not Found\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"404: Not Found!\r\n"
			"%s", message);
} else if ( which == 500 ) {
	sprintf(buffer, "HTTP/1.0 500 Internal Server Error\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"500: Internal Server Error!\r\n"
			"%s", message);
} else if ( which == 400 ) {
	sprintf(buffer, "HTTP/1.0 400 Bad Request\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"400: Not Found!\r\n"
			"%s", message);
} else if ( which == 601 ) {
	sprintf(buffer, "HTTP/1.0 601 Remote Authen Fail\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"601: Remote Authen Fail!\r\n"
			"%s", message);

} else {
	sprintf(buffer, "HTTP/1.0 %d Error\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"501: Not Implemented!\r\n"
			"%s", which, message);
}

	write(fd, buffer, strlen(buffer));
	fsync(fd);
}

/******************************************************************************
 Description.: Send HTTP header and copy the content of a file. To keep things
 simple, just a single folder gets searched for the file. Just
 files with known extension and supported mimetype get served.
 If no parameter was given, the file "index.html" will be copied.
 Input Value.: * fd.......: filedescriptor to send data to
 * parameter: string that consists of the filename
 * id.......: specifies which server-context is the right one
 Return Value: -
 ******************************************************************************/
void send_file(int id, int fd, char *parameter) {
	char buffer[BUFFER_SIZE] = { 0 };
	char *extension, *mimetype = NULL;
	int i, lfd;
	config conf = servers[id].conf;

	//mls@dev03 110131 debug info
//  printf("sending FILE now...\n");

	/* in case no parameter was given */
	if (parameter == NULL || strlen(parameter) == 0)
		parameter = "index.html";

	/* find file-extension */
	if ((extension = strstr(parameter, ".")) == NULL) {
		send_error(fd, 400, "No file extension found");
		return;
	}

	/* determine mime-type */
	for (i = 0; i < LENGTH_OF(mimetypes); i++) {
		if (strcmp(mimetypes[i].dot_extension, extension) == 0) {
			mimetype = (char *) mimetypes[i].mimetype;
			break;
		}
	}

	/* in case of unknown mimetype or extension leave */
	if (mimetype == NULL) {
		send_error(fd, 404, "MIME-TYPE not known");
		return;
	}

	/* now filename, mimetype and extension are known */
	DBG("trying to serve file \"%s\", extension: \"%s\" mime: \"%s\"\n",
			parameter, extension, mimetype);

	/* build the absolute path to the file */
	strncat(buffer, conf.www_folder, sizeof(buffer) - 1);
	strncat(buffer, parameter, sizeof(buffer) - strlen(buffer) - 1);

	/* try to open that file */
	if ((lfd = open(buffer, O_RDONLY)) < 0) {
		printf("file %s not accessible\n", buffer);
		send_error(fd, 404, "Could not open file");
		return;
	}
	DBG("opened file: %s\n", buffer);

	/* prepare HTTP header */
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			"Content-type: %s\r\n"
			STD_HEADER
			"\r\n", mimetype);
	i = strlen(buffer);

	/* first transmit HTTP-header, afterwards transmit content of file */
	do {
		if (write(fd, buffer, i) < 0) {
			close(lfd);
			return;
		}
	} while ((i = read(lfd, buffer, sizeof(buffer))) > 0);

	/* close file, job done */
	close(lfd);
}

void send_log(int id, int fd, char *parameter) {
	char buffer[BUFFER_SIZE] = { 0 };
	char *extension, *mimetype = NULL;
	int i, lfd;
	config conf = servers[id].conf;

	//mls@dev03 110131 debug info
//  printf("sending FILE now...\n");

	/* in case no parameter was given */
	if (parameter == NULL || strlen(parameter) == 0)
		parameter = "index.html";

	/* find file-extension */
	if ((extension = strstr(parameter, ".")) == NULL) {
		send_error(fd, 400, "No file extension found");
		return;
	}

	/* determine mime-type */
	for (i = 0; i < LENGTH_OF(mimetypes); i++) {
		if (strcmp(mimetypes[i].dot_extension, extension) == 0) {
			mimetype = (char *) mimetypes[i].mimetype;
			break;
		}
	}

	/* in case of unknown mimetype or extension leave */
	if (mimetype == NULL) {
		send_error(fd, 404, "MIME-TYPE not known");
		return;
	}

	/* now filename, mimetype and extension are known */
	printf("trying to serve file \"%s\", extension: \"%s\" mime: \"%s\"\n",
			parameter, extension, mimetype);

	/* build the absolute path to the file */
	strncat(buffer, conf.www_folder, sizeof(buffer) - 1);
	strncat(buffer, parameter, sizeof(buffer) - strlen(buffer) - 1);

	/* try to open that file */
	if ((lfd = open(buffer, O_RDONLY)) < 0) {
		printf("file %s not accessible\n", buffer);
		send_error(fd, 404, "Could not open file");
		return;
	}
	DBG("opened file: %s\n", buffer);

	/* prepare HTTP header */
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			"Content-type: %s\r\n"
			"Content-Disposition: attachment; filename=\"log.tar.gz\"\r\n"
			STD_HEADER
			"\r\n", mimetype);
	i = strlen(buffer);

	/* first transmit HTTP-header, afterwards transmit content of file */
	do {
		if (write(fd, buffer, i) < 0) {
			close(lfd);
			return;
		}
	} while ((i = read(lfd, buffer, sizeof(buffer))) > 0);

	/* close file, job done */
	close(lfd);
}

void send_mini_device_status(int id, int fd, char *parameter) {
	char buffer[512] = { 0 };
	char strGet[64];
	char *ptr;
	int res;

	ptr = buffer;
	/* prepare HTTP header */
	sprintf(ptr, "HTTP/1.0 200 OK\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n");
	ptr += strlen(ptr);
	sprintf(ptr,"t=%d,c=%d,b=%d,v=%d,m=%d,r=%d",pglobal->in.cmd(IN_CMD_TEMPERATURE,strGet),
					  pglobal->in.cmd(IN_CMD_VALUE_CONTRACT,strGet),
					  pglobal->in.cmd(IN_CMD_VALUE_BRIGHTNESS,strGet),
					  pglobal->in.cmd(IN_CMD_GET_SPK_VOLUME,strGet),
					  pglobal->in.cmd(IN_CMD_MELODY_INDEX,strGet),
					  pglobal->in.cmd(IN_CMD_VALUE_RESOLUTION,strGet)
					  );
	res = write(fd, buffer, strlen(buffer));
	/* close file, job done */
}

void send_device_status(int id, int fd, char *parameter) {
	char buffer[512] = { 0 };
	char strGet[64];
	char *ptr;
	int res;

	ptr = buffer;
	/* prepare HTTP header */
	sprintf(ptr, "HTTP/1.0 200 OK\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n");
	ptr += strlen(ptr);
	sprintf(ptr,"<?xml version=\"1.0\" encoding=\"windows-1250\"?>\r\n<device_status>\r\n");
	ptr+=strlen(ptr);
	res = pglobal->in.cmd(IN_CMD_GET_VERSION,strGet);
	sprintf(ptr,"<FWVersion>%s</FWVersion>\r\n",strGet);
	ptr+=strlen(ptr);
	sprintf(ptr,"<Brightness>%d</Brightness>\r\n",pglobal->in.cmd(IN_CMD_VALUE_BRIGHTNESS,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<Contrast>%d</Contrast>\r\n",pglobal->in.cmd(IN_CMD_VALUE_CONTRACT,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<Temperature>%d</Temperature>\r\n",pglobal->in.cmd(IN_CMD_TEMPERATURE,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<VoxStatus>%d</VoxStatus>\r\n",pglobal->in.cmd(IN_CMD_VOX_GET_STATUS,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<VoxThresh>%d</VoxThresh>\r\n",pglobal->in.cmd(IN_CMD_VOX_GET_THRESHOLD,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<MelodyIndex>%d</MelodyIndex>\r\n",pglobal->in.cmd(IN_CMD_MELODY_INDEX,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"<SpkVolume>%d</SpkVolume>\r\n",pglobal->in.cmd(IN_CMD_GET_SPK_VOLUME,strGet));
	ptr+=strlen(ptr);
	sprintf(ptr,"</device_status>\r\n");
	ptr+=strlen(ptr);
	res = write(fd, buffer, strlen(buffer));
	/* close file, job done */
}


/******************************************************************************
 Description.: Perform a command specified by parameter. Send response to fd.
 Input Value.: * fd.......: filedescriptor to send HTTP response to.
 * parameter: contains the command and value as string.
 * id.......: specifies which server-context to choose.
 Return Value: -
 ******************************************************************************/
void command(int id, int fd, char *parameter, struct sockaddr_in client_address) {
	char buffer[BUFFER_SIZE] = { 0 }, *command = NULL, *svalue = NULL,
			*strValue, *command_id_string, *setup = NULL, *commandtmp;
	int i = 0, res = -1, ivalue = 0, len = 0, iret;
	char strGet[4096];
	//add by mlsdev008 11/11/2011
	char command_key[10];
	struct sockaddr_in server_address = client_address;
//  printf("parameter is: %s\n", parameter);
	memset(command_key,10,0);
	/* sanity check of parameter-string */
	if (parameter == NULL
			|| /* strlen(parameter) >= 100 ||*/strlen(parameter) == 0) {
		printf("parameter string looks bad\n");
		send_error(fd, 400, "Parameter-string of command does not look valid.");
		return;
	}


       if ((strValue = strstr(parameter, "sesskey=")) != NULL) {
               printf("%s\n",strValue);
               //commandtmp = strndup(command,strlen(command)-strlen(value));
               strValue += strlen("sesskey=");
//      len = strlen(strValue);
               len = strspn(strValue,
                       "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890");
//  printf("command after %s\n",command);
               if (len == 8)
               {
                   memcpy(command_key,strValue,8);
                   command_key[8]=0;
               }
               printf("Sesskey='%s\n", command_key);
       }

	/* search for required variable "command" */
	if ((command = strstr(parameter, "command=")) == NULL) {
		printf("no command specified\n");
		send_error(
				fd,
				400,
				"no GET variable \"command=...\" found, it is required to specify which command to execute");
		return;
	}

	/* allocate and copy command string */
	command += strlen("command=");
//  printf("command before %s\n",command);
	len = strspn(command,
			"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890.");
//  printf("command after %s\n",command);
	if ((command = strndup(command, len)) == NULL) {
		send_error(fd, 500, "could not allocate memory");
		printf("could not allocate memory\n");
		return;
	}

////////////////print command////////////////////////////////////////////////////////////////////////////
	printf("command string: %s\n", command);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
	/* find and convert optional parameter "value" */
	if ((svalue = strstr(parameter, "value=")) != NULL) {
		svalue += strlen("value=");
		len = strspn(svalue, "-1234567890");
		if ((svalue = strndup(svalue, len)) == NULL) {
			if (command != NULL)
				free(command);
			send_error(fd, 500, "could not allocate memory");
			LOG("could not allocate memory\n");
			return;
		}
		ivalue = MAX(MIN(strtol(svalue, NULL, 10), 999), -999);
		DBG("converted value form string %s to integer %d\n", svalue, ivalue);
		free(svalue);
	}

	//sang100923
//  printf("%s\n",parameter);
	if ((strValue = strstr(parameter, "setup=")) != NULL) {
		printf("%s\n",strValue);
		//commandtmp = strndup(command,strlen(command)-strlen(value));
		strValue += strlen("setup=");
//      len = strlen(strValue);
		len = strstr(strValue, " HTTP/") - strValue;
		printf("data setup len:%d\n", len);
		if ((setup = strndup(strValue, len)) == NULL) {
			if (command != NULL)
				free(command);
			send_error(fd, 500, "could not allocate memory");
			LOG("could not allocate memory\n");
			return;
		}
		//if(command != NULL) free(command);
		//command = commandtmp;
		printf("%s\n", setup);
	}

	if ((strValue = strstr(parameter, "dataApplet=")) != NULL) {
		printf("%s\n", command);
		//commandtmp = strndup(command,strlen(command)-strlen(value));
		strValue += strlen("dataApplet=");
//      len = strlen(strValue);
		len = strstr(strValue, " HTTP/") - strValue;
		printf("data Applet len:%d\n", len);
		if ((setup = strndup(strValue, len)) == NULL) {
			if (command != NULL)
				free(command);
			send_error(fd, 500, "could not allocate memory");
			LOG("could not allocate memory\n");
			return;
		}
		//if(command != NULL) free(command);
		//command = commandtmp;
		printf("%s\n", setup);
	}

	//mls@dev03 110131 add check to bypass command "dummy_request"
	if (strcmp("dummy_request", command) == 0) {
		free(command);
//	  printf("bypass dummy command\n");
		return;
	}

	/*
	 * determine command, try the input command-mappings first
	 * this is the interface to send commands to the input plugin.
	 * if the input-plugin does not implement the optional command
	 * function, a short error is reported to the HTTP-client.
	 */
	for (i = 0; i < LENGTH_OF(in_cmd_mapping); i++) {
		if (strcmp(in_cmd_mapping[i].string, command) == 0) {
			if (pglobal->in.cmd == NULL) {
				send_error(fd, 501, "input plugin does not implement commands");
				if (command != NULL)
					free(command);
				return;
			}

#if 0  // will on later for real development
			// THESE COMMANDS BELOW ONLY ACCEPTED FROM CENTRAL SERVER
			
			if (
					(CheckIPisLocalorRemoteorServer(client_address,pglobal->CurrentIP,pglobal->NetworkmaskIP) ==1)// Server IP
			)
			{

			    char* tmp = RetrieveSessionKey()+56;
			    printf("Compare ('%s' vs '%s')\n",tmp,command_key);
			    //force all command must pass this one
			    if (strcmp(command_key,RetrieveSessionKey()+56) != 0)
			    {
				send_error(fd, HTTP_AUTHEN_ERROR_IP_BLOCK, "This IP is blocked\n");
				if (command != NULL) free(command);
				if (setup != NULL) free (setup);
				return;
			    }


			}
#endif
//      printf("httpd cmd\n");
			//mls@dev03 100923 comment change input command ivalue->strValue
			if ((in_cmd_mapping[i].cmd == IN_CMD_CONFIG_WIFI_READ)
					|| (in_cmd_mapping[i].cmd == IN_CMD_GET_STORE_FOLDER)
					|| (in_cmd_mapping[i].cmd == IN_CMD_UAPCONFIG_READ)
					|| (in_cmd_mapping[i].cmd == IN_CMD_GET_VERSION)
					|| (in_cmd_mapping[i].cmd == IN_CMD_GET_DEFAULT_VERSION)
					) {
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			else if (in_cmd_mapping[i].cmd == IN_CMD_SERVER_GET_SESSION_KEY)
			{
				if(setup == NULL){
					res = -1;
					strGet[0]=0;
					goto return_status;
				}
				strcpy(strGet,setup);
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			else if (in_cmd_mapping[i].cmd == IN_CMD_SET_DELAY_OUTPUT)
			{
				if(setup == NULL){
					res = -1;
					strGet[0]=0;
					goto return_status;
				}
				output_delay_ms = atoi(setup);
				printf("Set Delay Output to %d ms\n",output_delay_ms);
			}
			
			else if (in_cmd_mapping[i].cmd == IN_CMD_GET_ROUTERS_LIST)
			{
				strGet[0] = 0;
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			else if (in_cmd_mapping[i].cmd == IN_CMD_CHECK_FW_UPGRADE)
			{
				strGet[0] = 0;
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			else if (in_cmd_mapping[i].cmd == IN_CMD_GET_MAC_ADDRESS)
			{
				strGet[0] = 0;
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			else if (in_cmd_mapping[i].cmd == IN_CMD_GET_MAC_ADDRESS_IN_FLASH)
			{
				strGet[0] = 0;
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strGet);
			}
			
			else {
				if (in_cmd_mapping[i].cmd == IN_CMD_VOX_ENABLE) {
					setup = inet_ntoa(server_address.sin_addr);
				}
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, setup);
				if(in_cmd_mapping[i].cmd==IN_CMD_CONFIG_HTTP_SAVE_USR_PW
								&& res == MLS_SUCCESS)
				{
					free(servers[id].conf.credentials);
					servers[id].conf.credentials=strdup(setup);
					//printf("New user/pass: %s\n",servers[id].conf.credentials);
				}

			}
			break;
		}

		//mls@dev03 20110730 rapbit motion control
		if (strstr(command, "move_")) {
//    	if ((strcmp(command,"move_stop")==0)&&
//			 (strcmp(in_cmd_mapping[i].string,"move_stop")==0))
//		{
//    		printf("-> Motion stop");
//		    res = pglobal->in.cmd(in_cmd_mapping[i].cmd, NULL);
//		}
//    	else

			if (strstr(command, in_cmd_mapping[i].string)) {
				char strduty[4];
				char* pduty;

				pduty = strstr(parameter, " HTTP/") - 3;
				strduty[0] = *pduty;
				strduty[1] = *(pduty + 1);
				strduty[2] = *(pduty + 2);
				strduty[3] = '\0';

//			printf("==> %s 	factor %s\n",in_cmd_mapping[i].string,strduty);
				res = pglobal->in.cmd(in_cmd_mapping[i].cmd, strduty);
			}

		}
	}

	/* check if the command is for the output plugin itself */
//  for ( i=0; i < LENGTH_OF(out_cmd_mapping); i++ ) {
//    if ( strcmp(out_cmd_mapping[i].string, command) == 0 ) {
////      printf("hhtpd output %s\n",out_cmd_mapping[i].string);
//      res = output_cmd(id, out_cmd_mapping[i].cmd, ivalue);
//      break;
//    }
//  }

	/* Send HTTP-response */
	//mls@dev03 20101012
return_status:
	if ((in_cmd_mapping[i].cmd == IN_CMD_CONFIG_WIFI_READ)
			|| (in_cmd_mapping[i].cmd == IN_CMD_GET_STORE_FOLDER)
			|| (in_cmd_mapping[i].cmd == IN_CMD_UAPCONFIG_READ)
			|| (in_cmd_mapping[i].cmd == IN_CMD_GET_VERSION)
			|| (in_cmd_mapping[i].cmd == IN_CMD_GET_DEFAULT_VERSION)
			) {
sprintf	(buffer, "HTTP/1.0 200 OK\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"%s: %s", command, strGet);
}

else if (in_cmd_mapping[i].cmd == IN_CMD_GET_REG)
{
//	  printf("CMD: %s %d\n",command,res);
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"%s: at Address %s value %08X", command, setup, (unsigned int) res);
}
else if (in_cmd_mapping[i].cmd == IN_CMD_SERVER_GET_SESSION_KEY)
{
	printf("Special output for Get_Session_Key\n");
	if (res != 0)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"ERROR %d", res);
	}
	else // correct value
	{
		char sessionkey[65];
		printf("Get cache session key\n");
		printf("Session Key = %s\n ",strGet);
		//use to get the cache version out
		//mlsAuthen_GenerateSessionKey(NULL,NULL,sessionkey);
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%s", strGet);

	}

}

else if (in_cmd_mapping[i].cmd == IN_CMD_GET_ROUTERS_LIST)
{
	printf("Special output for routers list\n");
	if (res != 0)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"ERROR %d", res);
	}
	else // correct value
	{
		
		FILE * fp = fopen(strGet, "r");
		if(fp == NULL)
		{
			printf("Error: open %s file \n",strGet);
			sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"ERROR non-exist");
		}else
		{
			sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/xml\r\n"
				STD_HEADER
				"\r\n");
			if (send(fd, buffer, strlen(buffer),0) < 0) {
				printf("Error: writing socket\n");
			}else{
			  while(feof(fp)==0)
			  {
				  i= fread(buffer,1,BUFFER_SIZE,fp);
				  if(i < 0)
				  {
					  printf("Error: reading %s file\n",strGet);
					  break;
				  }
				  if (send(fd, buffer, i,0) < 0) {
					  printf("Error: writing socket\n");
					  break;
				  }
			  }
			  fclose(fp);
			  buffer[0] = 0;
			}
		}
		//TODO
	}
}
else if (in_cmd_mapping[i].cmd == IN_CMD_CHECK_FW_UPGRADE)
{
	if (res == 0)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%d", res);
	}
	else // correct value
	{
		printf("strGet = '%s'\n",strGet);
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%s",strGet);
		//TODO
	}
}

else if (in_cmd_mapping[i].cmd == IN_CMD_GET_MAC_ADDRESS)
{
	if (res == 0)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%d", res);
	}
	else // correct value
	{
		printf("strGet = '%s'\n",strGet);
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%s",strGet);
		//TODO
	}
}
else if (in_cmd_mapping[i].cmd == IN_CMD_GET_MAC_ADDRESS_IN_FLASH)
{
	if (res == 0)
	{
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%d", res);
	}
	else // correct value
	{
		printf("strGet = '%s'\n",strGet);
		sprintf(buffer, "HTTP/1.0 200 OK\r\n"
				"Content-type: text/plain\r\n"
				STD_HEADER
				"\r\n"
				"%s",strGet);
		//TODO
	}
}


else
{
//	  printf("CMD: %s %d\n",command,res);
	sprintf(buffer, "HTTP/1.0 200 OK\r\n"
			"Content-type: text/plain\r\n"
			STD_HEADER
			"\r\n"
			"%s: %d", command, res);
//	  if(strcmp(command,"setup_wireless_save")==0)
//	  {
//		  printf("write http setup_wireless_save return %d\n",res);
//		  iret = -1;
//		  while (iret<=0)
//		  {
//			  iret = write(fd, buffer, strlen(buffer));
//			  printf("print response save wireless setting\n");
//		  }
//	  }
}
	iret = write(fd, buffer, strlen(buffer));

	if (command != NULL)
		free(command);
}





/******************************************************************************
 Description.: Serve a connected TCP-client. This thread function is called
 for each connect of a HTTP client like a webbrowser. It determines
 if it is a valid HTTP request and dispatches between the different
 response options.
 Input Value.: arg is the filedescriptor and server-context of the connected TCP
 socket. It must have been allocated so it is freeable by this
 thread function.
 Return Value: always NULL
 ******************************************************************************/
/* thread for clients that connected to this server */
void *client_thread(void *arg) {
	int cnt;
	char org_buffer[BUFFER_SIZE];
	char buffer[BUFFER_SIZE] = { 0 }, *pb = buffer;
	char debugline[256];
	iobuffer iobuf;
	request req;
	int isRemote = 0;
	cfd lcfd; /* local-connected-file-descriptor */
	/* we really need the fildescriptor and it must be freeable by us */
	if (arg != NULL) {
		memcpy(&lcfd, arg, sizeof(cfd));
		free(arg);
	} else
		return NULL;
	total_thread_count++;

	if (pglobal->FlagAdhocMode == 1) {
		// in uAP mode there is a client connect to the system there for need to switch LED to ON only
		// borrow command to set the LED
		//pglobal->in.cmd(IN_CMD_LED_EAR_ON, NULL);
		adhocmode = 1;
		pglobal->FlagAdhocMode = 0;
	}

	/* initializes the structures */
	init_iobuffer(&iobuf);
	init_request(&req);

	/* What does the client want to receive? Read the request. */
	memset(buffer, 0, sizeof(org_buffer));
	if ((cnt = _readline(lcfd.fd, &iobuf, org_buffer, sizeof(org_buffer) - 1, 5))
			== -1) {
		close(lcfd.fd);
		total_thread_count--;
		return NULL;
	}

////////////////////////////////////////////////////////////////////////////////
	//mls@dev03 100924
	 printf("Org client thread : %s\n",org_buffer);
	 url_decode(org_buffer,buffer);
	 printf("After URL Encode : %s\n",buffer);
////////////////////////////////////////////////////////////////////////////////

	/* determine what to deliver */
	if (strstr(buffer, "GET /?action=snapshot") != NULL) {
		req.type = A_SNAPSHOT;
	} else if (strstr(buffer, "GET /?action=log") != NULL) {
		req.type = A_LOG;
	} else if (strstr(buffer, "GET /?action=device_status") != NULL) {
		req.type = A_STATUS;
	} else if (strstr(buffer, "GET /?action=mini_device_status") != NULL) {
		req.type = A_MINI_STATUS;
	} else if (strstr(buffer, "GET /?action=stream") != NULL) {
		req.type = A_STREAM;
	}

	//mls@dev03 100929 video+audio stream
	else if (strstr(buffer, "GET /?action=appletvastream") != NULL) {
		req.type = A_APPLETVASTREAM;
	}
	//mls@dev03 101013 video stream
	else if (strstr(buffer, "GET /?action=appletvstream") != NULL) {
		req.type = A_APPLETVSTREAM;
	}

	else if (strstr(buffer, "GET /?action=appletastream") != NULL) {
		req.type = A_APPLETASTREAM;
	}

	else if (strstr(buffer, "GET /?action=command") != NULL) {
		int len;
		req.type = A_COMMAND;

//    printf("%s \n",buffer);
		/* advance by the length of known string */
		if ((pb = strstr(buffer, "GET /?action=command")) == NULL) {
			printf("HTTP request seems to be malformed\n");
			send_error(lcfd.fd, 400, "Malformed HTTP request");
			close(lcfd.fd);
			total_thread_count--;
			return NULL;
		}
		pb += strlen("GET /?action=command");
		req.parameter = strdup(pb);
		if (req.parameter == NULL) {
			printf("Cant malloc len=%d\n",len);
			exit(EXIT_FAILURE);
		}
		DBG("command parameter (len: %d): \"%s\"\n", len, req.parameter);
	} else {
		int len;

		DBG("try to serve a file\n");
		req.type = A_FILE;

		if ((pb = strstr(buffer, "GET /")) == NULL) {
			printf("HTTP request seems to be malformed\n");
			send_error(lcfd.fd, 400, "Malformed HTTP request");
			close(lcfd.fd);
			total_thread_count--;
			return NULL;
		}

		pb += strlen("GET /");
		len =
				MIN(
						MAX(
								strspn(
										pb,
										"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._-1234567890"),
								0), 100);
		req.parameter = malloc(len + 1);
		if (req.parameter == NULL) {
			printf("Cant malloc len=%d\n",len);
			exit(EXIT_FAILURE);
		}
		memset(req.parameter, 0, len + 1);
		strncpy(req.parameter, pb, len);

		DBG("parameter (len: %d): \"%s\"\n", len, req.parameter);
	}

	// IMPLEMENT REMOTE AUTHENTICATION
	//

	// WHY DONT CHECK IN ALL COMMANDS
	// Put there till changes from Client
	// Now only check in VIEW STREAM

	printf("Connect %s\n", inet_ntoa(lcfd.connecter_addr.sin_addr));

	if (mlsIcam_IsinUAPMode_fp() && !mlsIcam_IsVideoTestFinish_fp())
	{
	    if (req.type != A_COMMAND)
	    {
		printf("Sorry in UAP view Stream\n");
		send_error(lcfd.fd, 400, "View Stream is prohibited");
		close(lcfd.fd);
		total_thread_count--;
		return NULL;

	    }
	}

#if 0  
	if (CheckIPisLocalorRemoteorServer(lcfd.connecter_addr,pglobal->CurrentIP,pglobal->NetworkmaskIP) == 1) // Remote View
#else
	if ((CheckIPisLocalorRemoteorServer(lcfd.connecter_addr, pglobal->CurrentIP,
			pglobal->NetworkmaskIP) == 1)
			&& ((req.type == A_APPLETVASTREAM) || (req.type == A_APPLETVSTREAM)
					|| (req.type == A_STREAM) || (req.type == A_SNAPSHOT) || (req.type == A_APPLETASTREAM)  )) // Remote View
#endif
			{
		//char ServerKey[65];
		char* SessionKey;
		MasterKey_st MKey;
		char CompareSessionKey[65];
		int len;
		int res;

		printf("Remote Session Requested 123. '%s'\n", buffer);
		/* find and convert optional parameter "value" */

		if ((SessionKey = strstr(buffer, "remote_session=")) != NULL) {
			printf("Enter Session Key parsing\n");
			SessionKey += strlen("remote_session=");
			len = strspn(SessionKey, "ABCDEF1234567890");
			printf("len of String = %d\n", len);
			if (len != 64) // SHA has 64 characters
					{
				send_error(lcfd.fd, HTTP_AUTHEN_ERROR_SESSION_KEY_NOT_CORRECT,
						"Session Key: format not correct");
				close(lcfd.fd);
				free_request(&req);
				total_thread_count--;
				return NULL;

			}
			if ((SessionKey = strndup(SessionKey, len)) == NULL) {
				send_error(lcfd.fd, 500, "could not allocate memory");
				LOG("could not allocate memory\n");
				close(lcfd.fd);
				free_request(&req);
				total_thread_count--;
				return NULL;
			}

		} else {
			printf("Not found session key2\n");
			send_error(lcfd.fd, HTTP_AUTHEN_ERROR_SESSION_KEY_NOT_CORRECT,
					"Lacking Session Key");
			close(lcfd.fd);
			free_request(&req);
			total_thread_count--;
			return NULL;
		}
/*
		if (TimerCheck() == 0) // timeout already ?
				{
			// clean remote IP
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			memset(pglobal->RandomKey, 0, 9);
			pglobal->RemoteIPFlag = 0;
			memset(&pglobal->RemoteIP, 0, sizeof(pglobal->RemoteIP));
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			printf("Camera Timeout1\n");
			send_error(lcfd.fd, HTTP_AUTHEN_ERROR_TIMEOUT, "Camera Timeout1");
			close(lcfd.fd);
			free_request(&req);
			free(SessionKey);
			total_thread_count--;
			return NULL;
		}
*/
		// is cam ready?
		if (pglobal->RemoteIPFlag > 0) // Have 1 Remote IP already inside
				{
			printf("Camera Not Available\n");
			send_error(lcfd.fd, HTTP_AUTHEN_ERROR_CAMERA_NOT_AVAILABLE,
					"Not Available");
			close(lcfd.fd);
			free_request(&req);
			free(SessionKey);
			total_thread_count--;
			return NULL;
		}

		res = ReadMasterKey(&MKey);
		if (res == MLS_ERR_WIFI_WRONGCRC) {
			// TODO: WHAT HAPPENED???? JOHN LE
			// Force to reload by Erasing the Wifi Config????
			printf("********************************MASTERKEY: CRC ERROR\n");
		}
		pthread_mutex_lock(&(pglobal->blinkled_mutex));
		strncpy(MKey.RandomKey, pglobal->RandomKey, 9);
		pthread_mutex_unlock(&(pglobal->blinkled_mutex));
		MKey.RandomKey[8] = '\0';
		ResetCreateSessionKeyProcess();
		//printf("MasterKey = '%s'\n", MKey.MasterKey);
		printf("RandomKey = '%s'\n", MKey.RandomKey);
		//GenerateSessionKey(MKey.RandomKey, &lcfd.connecter_addr,
		//		CompareSessionKey);
		GenerateSessionKey(MKey.RandomKey, NULL,
				CompareSessionKey);
		//ClearRemoteIP();
		//printf("SessionKey = '%s'\n", CompareSessionKey);
		//printf("Server Send SessionKey = '%s'\n", SessionKey);
		if (strncmp(SessionKey, CompareSessionKey, 64)) {
			printf("SessionKey Not Match\n");
			send_error(lcfd.fd, HTTP_AUTHEN_ERROR_SESSION_KEY_NOT_MATCH,
					"Camera SessionKey Not Matched\n");
			close(lcfd.fd);
			free_request(&req);
			free(SessionKey);
			total_thread_count--;
			return NULL;
		}
		free(SessionKey);
		isRemote = 1;

		// set socket timeout to increase it bigger
		{
			struct timeval tv;
			int size;
			if (getsockopt(lcfd.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, (socklen_t*)&size) == 0) {
				printf("Timeout = %d-%d\n",(int) tv.tv_sec, (int)tv.tv_usec);
			} else {
				perror("Get SockOpt error\n");
			}
			tv.tv_sec = 5;
			tv.tv_usec = 0;
			if (setsockopt(lcfd.fd, SOL_SOCKET, SO_SNDTIMEO, &tv,
					sizeof(struct timeval)) == 0) {
				tv.tv_sec = 0;
				if (getsockopt(lcfd.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, (socklen_t*)&size)
						== 0) {
					printf("Timeout = %d-%d\n",(int) tv.tv_sec, (int)tv.tv_usec);
				}
			} else {
				perror("Set Sockopt error\n");
			}
		}
	}

	if (CheckIPisLocalorRemoteorServer(lcfd.connecter_addr, pglobal->CurrentIP,pglobal->NetworkmaskIP) == 3)
	{
	    isRemote = 2;  // different solution for Localhost 
	}


	/*
	 * parse the rest of the HTTP-request
	 * the end of the request-header is marked by a single, empty line with "\r\n"
	 */
	do {
		memset(buffer, 0, sizeof(buffer));

		if ((cnt = _readline(lcfd.fd, &iobuf, buffer, sizeof(buffer) - 1, 5))
				== -1) {
			free_request(&req);
			close(lcfd.fd);
			total_thread_count--;
			return NULL;
		}

		if (strstr(buffer, "User-Agent: ") != NULL) {
			req.client = strdup(buffer + strlen("User-Agent: "));
		} else if ((strstr(buffer, "Authorization: Basic ") != NULL) 
			|| (strstr(buffer, "authorization: Basic ") != NULL)) {
			req.credentials = strdup(buffer + strlen("Authorization: Basic "));
			decodeBase64(req.credentials);
			//printf("username:password: %s\n", req.credentials);
		}

	} while (cnt > 2 && !(buffer[0] == '\r' && buffer[1] == '\n'));

	/* check for username and password if parameter -c was given. Only checked in case of Local Remote Detected */
	if ((lcfd.pc->conf.credentials != NULL) && (adhocmode == 1) && req.type != A_LOG)
//			&& (CheckIPisLocalorRemoteorServer(lcfd.connecter_addr,
//					pglobal->CurrentIP, pglobal->NetworkmaskIP) == 0)) 
    {
		if (req.credentials == NULL
				|| strcmp(lcfd.pc->conf.credentials, req.credentials) != 0) {
			printf("access denied\n");
			printf("**** Correct is '%s', u give '%s'\n",lcfd.pc->conf.credentials,req.credentials); 
			send_error(lcfd.fd, 401,
					"username and password do not match to configuration");
			close(lcfd.fd);
			if (req.parameter != NULL)
				free(req.parameter);
			if (req.client != NULL)
				free(req.client);
			if (req.credentials != NULL)
				free(req.credentials);
			total_thread_count--;
			return NULL;
		}
		printf("access granted\n");
	}

	/* now it's time to answer */
	switch (req.type) {
	case A_LOG: {
		char filename[32];
		{
		    printf("access denied\n");
			send_error(lcfd.fd, 401,
					"username and password do not match to configuration");
			close(lcfd.fd);
			if (req.parameter != NULL)
				free(req.parameter);
			if (req.client != NULL)
				free(req.client);
			if (req.credentials != NULL)
				free(req.credentials);
			total_thread_count--;
			return NULL;
		}
		
		
	}
		break;

	case A_STATUS:
		send_device_status(lcfd.pc->id, lcfd.fd,NULL);
		break;
	case A_MINI_STATUS:
		send_mini_device_status(lcfd.pc->id, lcfd.fd,NULL);
		break;
	
	case A_SNAPSHOT:
		DBG("Request for snapshot\n");
		send_snapshot(lcfd.fd);
		break;
	case A_STREAM:
		DBG("Request for stream\n");
#if defined (MS_LIMIT_CLIENT)
		if(pglobal->nbClient == 0)
		{
			pglobal->nbClient = 1;
			send_stream(lcfd.fd,isRemote);
			pglobal->nbClient = 0;
		}
#else
		send_stream(lcfd.fd, isRemote);
#endif
		break;
		//mls@dev03 100929
	case A_APPLETVASTREAM:
#if defined (MS_LIMIT_CLIENT)
		if(pglobal->nbClient == 0)
		{
			pglobal->nbClient = 1;
			send_applet_vastream(lcfd.fd,isRemote);
			pglobal->nbClient = 0;
			printf("end streaming video + audio !!!\n");
		}
#else
		send_applet_vastream(lcfd.fd, isRemote);
#endif
		break;

	case A_APPLETASTREAM:
#if defined (MS_LIMIT_CLIENT)
		if(pglobal->nbClient == 0)
		{
			pglobal->nbClient = 1;
			send_applet_astream(lcfd.fd,isRemote);
			pglobal->nbClient = 0;
			printf("end streaming audio !!!\n");
		}
#else
		send_applet_astream(lcfd.fd, isRemote);
#endif
		break;


	case A_APPLETVSTREAM:
#if defined (MS_LIMIT_CLIENT)
		if(nbClient == 0)
		{
			nbClient = 1;
			send_applet_vstream(lcfd.fd,isRemote);
			nbClient = 0;
		}
#else
		send_applet_vstream(lcfd.fd, isRemote);
#endif
		break;
	case A_COMMAND:
		if (lcfd.pc->conf.nocommands) {
			send_error(lcfd.fd, 501,
					"this server is configured to not accept commands");
			break;
		}
		command(lcfd.pc->id, lcfd.fd, req.parameter, lcfd.connecter_addr);
		break;
	case A_FILE:
		if (lcfd.pc->conf.www_folder == NULL)
			send_error(lcfd.fd, 501, "no www-folder configured");
		else
			send_file(lcfd.pc->id, lcfd.fd, req.parameter);
		break;
	default:
		DBG("unknown request\n");
	}

	close(lcfd.fd);
	free_request(&req);
	total_thread_count--;
	printf("leaving\n");
	return NULL;
}

/******************************************************************************
 Description.: This function cleans up ressources allocated by the server_thread
 Input Value.: arg is not used
 Return Value: -
 ******************************************************************************/
void server_cleanup(void *arg) {
	context *pcontext = arg;

	OPRINT("cleaning up ressources allocated by server thread #%02d\n",
			pcontext->id);

	close(pcontext->sd);
}

/******************************************************************************
 Description.: Open a TCP socket and wait for clients to connect. If clients
 connect, start a new thread for each accepted connection.
 Input Value.: arg is a pointer to the globals struct
 Return Value: always NULL, will only return on exit
 ******************************************************************************/
void *server_thread(void *arg) {
	struct sockaddr_in addr, client_addr;
	int on;
	pthread_t client;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	context *pcontext = arg;
	pglobal = pcontext->pglobal;

	/* try to find optional command */
	ResetCreateSessionKeyProcess = mlsAuthen_ResetCreateSessionKeyProcess;
	ReadMasterKey = mlsAuthen_ReadMasterKey;
	GenerateSessionKey = mlsAuthen_GenerateSessionKey;
	CheckIPisLocalorRemoteorServer = mlsAuthen_CheckIPisLocalorRemoteorServer;
	TimerCheck = mlsAuthen_TimerCheck;
	ClearRemoteIP = mlsAuthen_ClearRemoteIP;
	SetRemoteIP = mlsAuthen_SetRemoteIP;
	RetrieveSessionKey = mlsAuthen_RetrieveSessionKey;
    //! Assign Postoffice action
    PostOffice_ReceiveMail_fp= PostOffice_ReceiveMail;
    PostOffice_MarkMailRead_fp= PostOffice_MarkMailRead;
    PostOffice_Register_fp= PostOffice_Register; // return MailboxID
    PostOffice_DeRegister_fp= PostOffice_DeRegister;
    PostOffice_AvailableTotalMail_fp = PostOffice_AvailableTotalMail;
    mlsIcam_IsVideoTestFinish_fp = mlsIcam_IsVideoTestFinish;
    mlsIcam_IsinUAPMode_fp = mlsIcam_IsinUAPMode;
	/* set cleanup handler to cleanup ressources */
	pthread_cleanup_push(server_cleanup, pcontext);

	/* open socket for server */
	pcontext->sd = socket(PF_INET, SOCK_STREAM, 0);
	if (pcontext->sd < 0) {
		fprintf(stderr, "socket failed\n");
		exit(EXIT_FAILURE);
	}

	/* ignore "socket already in use" errors */
	on = 1;
	if (setsockopt(pcontext->sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))
			< 0) {
		perror("setsockopt(SO_REUSEADDR) failed");
		exit(EXIT_FAILURE);
	}

	/* perhaps we will use this keep-alive feature oneday */
	/* setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)); */

	/* configure server address to listen to all local IPs */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = pcontext->conf.port; /* is already in right byteorder */
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(pcontext->sd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		perror("bind");
		OPRINT("%s(): bind(%d) failed", __FUNCTION__,
				htons(pcontext->conf.port));
		closelog();
		exit(EXIT_FAILURE);
	}

	/* start listening on socket */
	if (listen(pcontext->sd, 1) != 0) {
		fprintf(stderr, "listen failed\n");
		exit(EXIT_FAILURE);
	}

	//mls@dev03 110131 debug info
	printf("Web server is ready and listenning...\n");

	/* create a child for every client that connects */
	while (!pglobal->stop) {
		//int *pfd = (int *)malloc(sizeof(int));
		cfd *pcfd = malloc(sizeof(cfd));

		if (pcfd == NULL) {
			fprintf(stderr,
					"failed to allocate (a very small amount of) memory\n");
			exit(EXIT_FAILURE);
		}

//    if (pcontext->countClient<pcontext->maxClient)//mls@dev03
		{
//    	printf("number of client %d \n",pcontext->countClient);
//    	pcontext->countClient++;

			DBG("waiting for clients to connect\n");
			pcfd->fd = accept(pcontext->sd, (struct sockaddr *) &client_addr,
					&addr_len);
			pcfd->pc = pcontext;
			//add by mlsdev008 11/11/2011
			pcfd->connecter_addr = client_addr; //get ip address
			/* start new thread that will handle this TCP connected client */
			DBG(
					"create thread to handle client that just established a connection\n");

			//TODO mls@dev03 comment syslog
			//    syslog(LOG_INFO, "serving client: %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

			//mls@dev03 110131 increase number request of client
			pthread_mutex_lock(&(pglobal->blinkled_mutex));
			pglobal->countRequestClient++;
//    	printf("receive a connect...\n");
			pthread_mutex_unlock(&(pglobal->blinkled_mutex));
			if (total_thread_count >= MAX_THREAD_COUNT)
			{
				printf("Over Maximum Thread\n");
				close(pcfd->fd);
				free(pcfd);
				continue;
			}
			if (total_appletva_count >= MAX_APPLETVA_STREAM_COUNT)
			{
			    	printf("Over Maximum AppletVA Thread\n");
				close(pcfd->fd);
				free(pcfd);
				continue;
			
			}
			if (pthread_create(&client, NULL, &client_thread, pcfd) != 0) {
				printf("could not launch another client thread\n");
				close(pcfd->fd);
				free(pcfd);
				continue;
			}
			pthread_detach(client);

//		pcontext->countClient--;mls@dev03
		}
	}

	DBG("leaving server thread, calling cleanup function now\n");
	pthread_cleanup_pop(1);

	return NULL;
}

