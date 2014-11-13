/*
 * mlsflash.c
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/watchdog.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include "mlsInclude.h"
#include "postoffice.h"



//#define _DEBUG
#define ENABLE_RETRICT_NO_MAILS
#ifdef ENABLE_RETRICT_NO_MAILS
	#define MAX_BUFFER_DELAY 30
#endif
#ifdef	_DEBUG
	#include "../../debug.h"

#else
	#define printf(...)
#endif

static PostOfficeContext context;
static void PostOffice_FreeMail(pMailMessage pMail);
void PostOfficeReportInfo()
{
	pMailMessage pMail;
	//int totalMail = 0;
	pMail = context.pfirstMessage;
	printf("*******REPORT*******\n");
	while(pMail!=NULL)
	{
		printf("Msg Id %d - Content %d. Counter = %d\n",pMail->id,*(int*)pMail->data,pMail->counter);
		pMail = pMail->pnextMsg;
	}
	printf("******************\n");
}

void PostOfficeInit()
{
	memset(&context,sizeof(context),0);
	context.message_id = 0;
	context.pfirstMessage = context.plastMessage = NULL;
	context.box_counter = 0;
	  if (pthread_mutex_init(&context.mutex,NULL) != 0)
	  {
		  printf("**** CANT CREATE MUTEX\n");
	  }
}

void PostOfficeDeInit()
{

}

int PostOffice_SendMail (char* data, int datalen)
{
	pMailMessage pMail;
	int ret = 0, gap_count,i;
	pthread_mutex_lock(&context.mutex);
	if (context.box_counter == 0)
	{
		//printf("SendMail: Ignore\n");
		ret = 0;
		goto EXIT;
		//return 0; // nothing to store
	}
	pMail = malloc (sizeof(MailMessage));
	if (pMail == NULL)
	{
		printf("Not enough Memory to create a Message\n");
		ret = -1;
		goto EXIT;
	}
	pMail->id = context.message_id;
	pMail->pcontext = &context;
	pMail->size = datalen;
	pMail->data = malloc(datalen);
    if(pMail->data == NULL)
    {
        ret = -1;
        goto EXIT;
    }
    memcpy(pMail->data, data, datalen);
	pMail->counter = context.box_counter;
	pMail->pnextMsg = NULL;
	gettimeofday(&pMail->ts,NULL);
	printf("SendMail: id %d size %d\n",pMail->id,pMail->size);
	if (context.pfirstMessage == NULL)  // first data
	{
		context.pfirstMessage = context.plastMessage = pMail;
	}
	else
	{
		context.plastMessage->pnextMsg = pMail;
		context.plastMessage= pMail;
	}
	context.message_id++;
	//printf("Message Id = %d\n",context.message_id);
	//!Retrict the number of mails in mailbox
#ifdef  ENABLE_RETRICT_NO_MAILS
	gap_count = context.plastMessage->id - context.pfirstMessage->id ;
	if(gap_count < 0)
		gap_count = -gap_count;
	if(gap_count > MAX_BUFFER_DELAY)
	{
		//Free oldest mail
		pMail = context.pfirstMessage;
		context.pfirstMessage = context.pfirstMessage->pnextMsg;
		PostOffice_FreeMail(pMail);

		//Update MailBoxes
		for(i = 0; i < MAX_MAILBOX_NUMBER; i++)
		{
			if(context.boxlist[i].isused == 1)
			{
				
				 gap_count = context.plastMessage->id - context.boxlist[i].currentreadingMessageId;
				 if(gap_count > MAX_BUFFER_DELAY || gap_count < -MAX_BUFFER_DELAY)
				 {
				 	printf("Gap %d MailID=%d=count%d\n",MAX_BUFFER_DELAY,i,gap_count);
				 	context.boxlist[i].currentreadingMessageId +=1;
				 }
			}
		}
	}
#endif
EXIT:
	pthread_mutex_unlock(&context.mutex);
	return ret;
}


int PostOffice_Register()
{
	int i,ret;
	pthread_mutex_lock(&context.mutex);
	for (i=0;i<10;i++)
	{
		if (!context.boxlist[i].isused)
		{
			context.boxlist[i].isused = 1;
			context.boxlist[i].currentreadingMessageId = context.message_id;
			context.box_counter++;
			//printf("Reg with box %d\n",i);
			//printf("Wanna read message id %d\n",context.boxlist[i].currentreadingMessageId);
			ret = i;
			break;
		}
	}
	if (i==10) ret = -1;
	pthread_mutex_unlock(&context.mutex);
	return ret;
}

static void PostOffice_FreeMail(pMailMessage pMail)
{
	if (pMail->data)
	{
		//printf("[INT] Free mail %d\n",*(int*)pMail->data);
		free (pMail->data);
		pMail->data = NULL;
	}

	if (pMail)
	{
		free(pMail);
		pMail = NULL;
	}
}

int PostOffice_DeRegister(int BoxID)
{
//	int new_id=-1;
	pMailMessage pMail;
	int id = 0;

    pthread_mutex_lock(&context.mutex);
	id = context.boxlist[BoxID].currentreadingMessageId;
	pMail = context.pfirstMessage;
	if(context.boxlist[BoxID].isused == 0) //not using
	{
		pthread_mutex_unlock(&context.mutex);
		return -1;
	}
	// reduce the reading counter to all of the messages
	while(pMail!=NULL)
	{
		if (pMail->id >= id)
			break;
		pMail = pMail->pnextMsg;
	}
	//if (pMail == NULL)
		//return 0;

	// from this point go to the last need to minus the counter
	while (pMail != NULL)
	{
		pMail->counter--;

		//if (pMail->counter <= 0)
		//{
		//	new_id = pMail->id; // delete_id
		//}
		pMail = pMail->pnextMsg;
	}
	// free box now
	context.boxlist[BoxID].isused = 0;
	context.box_counter --;
	pthread_mutex_unlock(&context.mutex);
	return 0;
}

int PostOffice_ReceiveMail(char** pdata, int* pdatalen, int BoxID,struct timeval* diff_ts)
{
	pMailMessage pMail;
	int id = 0;
	int ret = 0;
	*pdatalen = 0;
	pthread_mutex_lock(&context.mutex);
	id = context.boxlist[BoxID].currentreadingMessageId;
	if(context.boxlist[BoxID].isused == 0)
	{
		ret = -1;
		goto EXIT;
	}
	if (context.message_id < id)
	{
		ret = -2;
		goto EXIT;
	}
	pMail = context.pfirstMessage;
	// reduce the reading counter to all of the messages
	//printf("Want to read ID %d ...........................\n", id);
	while(pMail!=NULL)
	{
		//printf("Scan id %d\n",pMail->id);
		if (pMail->id >= id) {
			//printf("Receive ID %d.............................\n",pMail->id);
			break;
		}
		pMail = pMail->pnextMsg;
	}
	if (pMail == NULL)
	{
		ret = -3; // nothing to get
		goto EXIT;
	}
	*pdata = pMail->data;
	*pdatalen = pMail->size;
	if (diff_ts != NULL)
	{
		struct timeval ts;
		gettimeofday(&ts,NULL);
		PostOffice_timeval_subtract(diff_ts,&ts,&(pMail->ts));
	}
	printf("BoxID %d: Receive Mail id %d\n",BoxID, pMail->id);
EXIT:
	pthread_mutex_unlock(&context.mutex);
	return ret;
}

int PostOffice_MarkMailRead(int BoxID)
{
	int new_id=-1;
	pMailMessage pMail;
	int id = 0;
	int ret = 0;

	pthread_mutex_lock(&context.mutex);
	id = context.boxlist[BoxID].currentreadingMessageId;
	if(context.boxlist[BoxID].isused == 0)
	{
		ret = -1;
		goto EXIT;
	}
	printf("MarkMail[%d]: Wanna mark id %d\n",BoxID,id);
	pMail = context.pfirstMessage;
	// reduce the reading counter to all of the messages
	while(pMail!=NULL)
	{
		if (pMail->id >= id)
			break;
		pMail = pMail->pnextMsg;
	}
	context.boxlist[BoxID].currentreadingMessageId=pMail->id+1;
	pMail->counter --;
	if (pMail->counter <= 0)
	{
		new_id = pMail->id;
	}
/*	
	
	if ( (context.message_id - id) > (MAX_BUFFER_DELAY+2))
	{
	    int k;
	    // why you need to store too much buffer like that. Need to freeze the mem too
	    // this thing is remote usecase problem
	    printf("Mailbox: Remote usecase issue. Cant store too much need to free data\n");
	    pMail = pMail->pnextMsg;
	    for (k = 0; k < MAX_BUFFER_DELAY ; k++)
	    {
		pMail -> counter --;
		if (pMail->counter <= 0)
		{
		    new_id = pMail->id;
		}
		pMail = pMail->pnextMsg;
	    }
	}
*/

	// clean up
	if (new_id != -1)
	{
		pMail = context.pfirstMessage;
		while(pMail!=NULL)
		{

			if (pMail->id <= new_id)
			{
				// free this Mail
				pMailMessage ptmpMail;
				printf("MarkMail[%d] Free mailid %d\n",BoxID,pMail->id);
				ptmpMail = pMail;
				pMail = pMail->pnextMsg;
				PostOffice_FreeMail(ptmpMail);
				context.pfirstMessage = pMail;
			}
			else
				pMail = pMail->pnextMsg;
		}
	}
EXIT:
	pthread_mutex_unlock(&context.mutex);
	return ret;
}


int PostOffice_AvailableTotalMail(int MailboxID, int* totallen, int* totalmail)
{
	pMailMessage pMail;
	int id = 0;
	int ret = 0;
	int _totallen,_totalmail;

	pthread_mutex_lock(&context.mutex);
	id = context.boxlist[MailboxID].currentreadingMessageId;
	if(context.boxlist[MailboxID].isused == 0)
	{
		ret = -1;
		goto EXIT;
	}
	//printf("Wanna mark %d\n",id);
	pMail = context.pfirstMessage;
	// reduce the reading counter to all of the messages
	_totalmail = 0;
	_totallen = 0;
	while(pMail!=NULL)
	{
		if (pMail->id >= id)
		{
			_totalmail++;
			_totallen += pMail->size;
			printf("AvailTotalmail[%d] id %d\n",MailboxID, pMail->id);
		}
		pMail = pMail->pnextMsg;
	}
	*totallen = _totallen;
	*totalmail = _totalmail;
EXIT:
	pthread_mutex_unlock(&context.mutex);
	return ret;

}

int PostOffice_timeval_subtract (struct timeval *result,struct timeval *x,struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}
