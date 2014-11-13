/*
 * mlsflash.h
 *
 *  Created on: Aug 23, 2011
 *      Author: root
 */

#ifndef POST_OFFICE_H_
#define POST_OFFICE_H_

#include <sys/time.h>

#define MAX_MAILBOX_NUMBER 10

typedef struct
{
    int MailboxID;
    int isused;
    int currentreadingMessageId;
}Mailbox;

typedef struct MailMessage
{
    int id;
    char* data;
    int size;
    int counter;
    struct MailMessage* pnextMsg;
    struct timeval ts;
    void* pcontext;
}MailMessage,*pMailMessage;

typedef struct
{
    int message_id;
    int box_counter;
    pMailMessage pfirstMessage;
    pMailMessage plastMessage;
    Mailbox boxlist[MAX_MAILBOX_NUMBER];
    pthread_mutex_t mutex;
}PostOfficeContext,*pPostOfficeContext;


int PostOffice_SendMail (char* data, int datalen);
int PostOffice_ReceiveMail(char** pdata, int* pdatalen,int MailboxID,struct timeval *diff_timeval);
int PostOffice_MarkMailRead(int MailboxID);
int PostOffice_AvailableTotalMail(int MailboxID, int* totallen, int* totalmail);
int PostOffice_Register(); // return MailboxID
int PostOffice_DeRegister(int MailboxID);
void PostOfficeInit();
void PostOfficeReportInfo();
int PostOffice_timeval_subtract (struct timeval *result,struct timeval *x,struct timeval *y);

#endif /* MLSFLASH_H_ */
