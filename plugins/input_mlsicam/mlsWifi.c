/*
 * mlsWifi.c
 *
 *  Created on: Oct 9, 2010
 *      Author: mlslnx003
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <net/if.h>
#include <limits.h>

#include <errno.h>
#include <signal.h>
#include <linux/watchdog.h>

#include "mlsWifi.h"
#include "mlsflash.h"
#include "mlsio.h"
#include "mlsicam.h"
#include "ping.h"
#include "sys_configs.h"
#include "nxcCRC32.h"
#include "../input.h"
#include "../../debug.h"



#define SSID_		"BLINK-"
#define INTERFACE	"ra0"
#define TIME_DELAY	10
#define WifiConfFile	"/etc/Wireless/RT2870AP/RT2870AP.dat"

#define CONNECTED	1
#define DISCONNECT	0

#define	BOOT_OPTION_DEV_NAME	"/dev/mtdblock3"
#define	BOOT_MAGIC_DATA_STR		"abcdefghij"

#define WIFI_DEBUG "WIFI:"

#define WIFI_CONFIGSPL_HEADER "ctrl_interface=/var/run/wpa_supplicant\r\n"\
							  "ctrl_interface_group=0\r\n"\
							  "ap_scan=1\r\n"\
							  "\n"\
							  "network={\r\n"

#define WIFI_CONFIGSPL_TAIL   "}\r\n"
#define	ROUTERS_LIST_FILE  "/tmp/routers_list"

struct wifi_info{
    char ssid[50]; 
    char bssid[50]; 
    char  auth_mode[20]; 
    char  quality[20]; 
    int signal_level;
    int noise_level; 
    int channel; 
    struct wifi_info * p_next;
};
struct wifi_info * wifilist_head = NULL, * wifilist_tail = NULL;
int wifilist_size = 0;
#define ROUTERS_LIST_XML  "/tmp/routers_list.xml"
#define INPUT_FILE "wifilist"


mlsErrorCode_t _mlsWifi_infra_checkconnectivity(icamWifiConfig_st *icamWifiConfig);
int getLocalIPAddress(char* IPStr);



int checkWifiInterface(){
	int retVal;
	FILE *fp = NULL;
	char line[500];
	char *pdes;
	
	fp = fopen("/proc/net/wireless", "r");
	if (fp == NULL)
	{
		printf(WIFI_DEBUG"error open /proc/net/wireless !\n");
		return MLS_ERR_OPENFILE;
	}

	retVal = MLS_ERR_WIFI_NOINTERFACE;
	

	while(!feof(fp))
	{
		fgets(line, 500, fp);

		pdes = strstr(line, "ra0");
		if(pdes == NULL)
		{
			continue;
		}
		else
		{

			retVal = MLS_SUCCESS; 
			break;
		}
	}
	
	fclose(fp);
	return retVal;
}
/**
 *
 *
 */
int  is_exist_router(struct wifi_info * wifi_p)
{
	struct wifi_info * wifi_tmp;
	wifi_tmp = wifilist_head;
	while(wifi_tmp)
	{
		if(strcmp(wifi_p->ssid, wifi_tmp->ssid) == 0)
		{
			return 1;
		}
		wifi_tmp = wifi_tmp -> p_next;
	}
	return 0;
}
/**
 *
 *
 */
int get_wifi_list(char * in_file)
{
    char line[500];
    char *pdes;
    int num;
    float num1;
    struct wifi_info * wifi_p = NULL;
    FILE * rfp ;

    rfp = fopen(in_file, "r");
    if (rfp == NULL)
    {
        printf("Error: open file %s\n",in_file);
        return -1;
    }

    while(!feof(rfp))
    {

		fgets(line, 500, rfp);

		pdes = strstr(line, "Cell ");
		if(pdes != NULL)
		{

		    if(wifi_p !=NULL)
		    {
		        wifi_p->p_next = NULL;
		        if(wifilist_tail == NULL)
		        {
		            wifilist_tail = wifi_p;
		            wifilist_head = wifi_p;
		            wifilist_size = 1;
		        }else if( is_exist_router(wifi_p) == 0)
		        {
		            wifilist_tail->p_next = wifi_p;
		            wifilist_tail = wifi_p;
		            wifilist_size ++;
		        }else
		        {
		        	free(wifi_p);
		        }
		    }

		    wifi_p =(struct wifi_info *) malloc(sizeof(struct wifi_info));
			sscanf(pdes,"Cell %d - Address: %s",&num,wifi_p->bssid);
		}else if(wifi_p != NULL)
		{

		    pdes = strstr(line, "ESSID:");
		    if(pdes != NULL)
		    {
		        sscanf(pdes,"ESSID:%48[^\n]",wifi_p->ssid);
		    }

		    pdes = strstr(line, "Frequency:");
		    if(pdes != NULL)
		    {
		        sscanf(pdes,"Frequency:%f GHz (Channel %d)",&num1,&wifi_p->channel);
		    }

		    pdes = strstr(line, "Quality:");
		    if(pdes != NULL)
		    {
		        sscanf(pdes+8,"%s  Signal level:%d dBm  Noise level:%d dBm",wifi_p->quality,&wifi_p->signal_level,&wifi_p->noise_level);
		    }else{
		    	pdes = strstr(line, "Quality=");
		    	if(pdes != NULL)
		    	{
		        	sscanf(pdes+8,"%s  Signal level=%d dBm  Noise level=%d dBm",wifi_p->quality,&wifi_p->signal_level,&wifi_p->noise_level);
		    	}

		    }

		    pdes = strstr(line, "Encryption key:");
		    if (pdes != NULL)
		    {
			pdes += 15;
			if (strstr(pdes,"on"))
			{
			    strcpy(wifi_p->auth_mode,"WEP");
			}
			else if (strstr(pdes,"off"))
			{
			    strcpy(wifi_p->auth_mode,"OPEN");
			}
			continue;
		    }
		    //IE: WPA Version 1
		    pdes = strstr(line, "IE:");

		    if(pdes != NULL)
		    {
		        char *pdes2 = strstr(pdes,"WPA2");
		        
		        if(pdes2 != NULL)
		        {
		            strcpy(wifi_p->auth_mode,"WPA2");
		        }else
		        {
		            pdes2 = strstr(pdes,"WPA");
		            if(pdes2 != NULL)
		            {
		                strcpy(wifi_p->auth_mode,"WPA");
		            }
		        }
		    }
		}
	}

	if(wifi_p !=NULL)
    {
        wifi_p->p_next = NULL;
        if(wifilist_tail == NULL)
        {
            wifilist_tail = wifi_p;
            wifilist_head = wifi_p;
            wifilist_size = 1;
        }else if( is_exist_router(wifi_p) == 0)
        {
            wifilist_tail->p_next = wifi_p;
            wifilist_tail = wifi_p;
            wifilist_size ++;
        }else
        {
        	free(wifi_p);
        }
    }
    fclose(rfp);
	return 0;
}


const char *				/* O - Entity name or NULL */
mxmlEntityGetName(int val)		/* I - Character value */
{
  switch (val)
  {
    case '&' :
        return ("amp");

    case '<' :
        return ("lt");

    case '>' :
        return ("gt");

    case '\"' :
        return ("quot");

    default :
        return (NULL);
  }
}

static int				/* O - 0 on success, -1 on failure */
write_xml_string(
    const char      *s,			/* I - String to write */
    char            *dest)			/* I - Write pointer */
{
  const char	*name;			/* Entity name, if any */
  char* d = dest;

  while (*s)
  {
    if ((name = mxmlEntityGetName(*s)) != NULL)
    {
      *d++ = '&';

      while (*name)
      {
		*d++ = *name;
        name ++;
      }
	  
	  *d++ = ';';
    }
    else
	{
	   *d++ = *s;
	}
    s ++;
  }
  *d=0;
  return (0);
}

/**
 *
 *
 */
int create_wifi_list_xmlfile(char * filename)
{
    FILE * wfp;
    int i;
    char xml_ssid[64];
    wfp = fopen(filename, "w");
    if(wfp < 0)
    {
        printf("sdfasdf\n");
        return -1;
    }
    //fprintf(wfp,"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n");
    //!Hai Nguyen Edited May 5th, 2013
    fprintf(wfp,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf( wfp,"<!-- date: 20120618 -->\n");
    fprintf( wfp,"<wifi_list version=\"1.0\">\n");
    fprintf( wfp,"<num_entries>%d</num_entries>\n",wifilist_size);
    struct wifi_info * tmp_p = wifilist_head;
    while(tmp_p !=NULL)
    {
	write_xml_string(tmp_p->ssid,xml_ssid);
        fprintf(wfp,"<wifi>\n\
<ssid>%s</ssid>\n\
<bssid>%s</bssid>\n\
<auth_mode>%s</auth_mode>\n\
<quality>%s</quality>\n\
<signal_level>%d</signal_level>\n\
<noise_level>%d</noise_level>\n\
<channel>%d</channel>\n\
</wifi>\n",xml_ssid,tmp_p->bssid,tmp_p->auth_mode,tmp_p->quality,tmp_p->signal_level,tmp_p->noise_level,tmp_p->channel);
        tmp_p = tmp_p->p_next;
    }
    
    fprintf( wfp,"</wifi_list>\n");
    fclose(wfp);
    return 0;
}
/**
 *
 *
 */
int clean_wifi_list(void)
{
    struct wifi_info * tmp_p;
    while(wifilist_head != NULL)
    {
        tmp_p = wifilist_head;
        wifilist_head = wifilist_head->p_next;
        free(tmp_p);
    }
    return 0;
}

uAPConfig_st * puAPConfig = NULL;

mlsErrorCode_t _mlsWifi_SetUAPConfigFile(void)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	FILE *fp = NULL;
	Char *pdes;
	Char line[500];
	//uAPConfig_st uAPConfig;

    fp = fopen("/etc/Wireless/RT2870AP/RT2870AP.dat", "r+");
    if (fp == NULL)
    {
        printf(WIFI_DEBUG"error open wifilist file !\n");
        return MLS_ERR_OPENFILE;
    }

    retVal = GetUAPConfig(&puAPConfig);


    while(!feof(fp))
    {
        fgets(line, 500, fp);

		pdes = strstr(line, "Channel=");
		if(pdes == NULL)
		{
			continue;//return while loop to read new line
		}
		else
		{

			fseek(fp,ftell(fp)-3,SEEK_SET);
			printf("	Change channel in config file to %02d\n",puAPConfig->channel);
			fprintf(fp,"%02d\n",puAPConfig->channel);
			break;
		}
    }

    fclose(fp);

	return retVal;
}

mlsErrorCode_t getMacAddr(struct ifreq *ifr)
{
	mlsErrorCode_t reVal = MLS_SUCCESS;
	int fd = -1;
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	 ifr->ifr_ifru.ifru_addr.sa_family = AF_INET;
	 strncpy(ifr->ifr_ifrn.ifrn_name, INTERFACE, IFNAMSIZ-1);
	 ioctl(fd, SIOCGIFHWADDR, ifr);
	 close(fd);
	return reVal;
}
mlsErrorCode_t mlsGetSSID(char *ssid)
{
	if(ssid == NULL)
	{
		printf("your buffer not available\n");
		return MLS_ERROR;
	}
	struct ifreq ifr;
	getMacAddr(&ifr);
	print("MAC ADDR: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[0],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[1],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[2],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);

	sprintf(ssid,"%s%.2x%.2x%.2x",SSID_,
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
	print("SSID:%s\n",ssid);
	return MLS_SUCCESS;
}
int mlsSetSSID()
{
	char SSID[127] = {'\0'},cmd[127] = {'\0'};
	if(mlsGetSSID(SSID) != MLS_SUCCESS)
	{
//		perror("get ssid error");
		return -1;
	}
	sprintf(cmd,"iwpriv ra0 set SSID=%s",SSID);
	if(system(cmd) < 0)
	{
		perror("run cmd to set ssid for wifi error");
		return -1;
	}
	return 0;
}
/**
 *
 *
 */
int start_adhoc_wifi(void)
{
	int iret;
	printf("try to unload uap wifi modules...\n");
	iret=system("ifconfig ra0 down");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("rmmod rtnet3070ap");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("rmmod rt3070ap");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("rmmod rtutil3070ap");
	if (iret < 0)
	{
		return -1;
	}
	printf("load sta wifi modules...\n");
	iret=system("sh /rt3070sta.sh");
	if (iret < 0)
	{
		return -1;
	}
	printf("setup adhoc wifi...\n");
	iret=system("iwconfig ra0 mode ad-hoc");
	if (iret < 0)
	{
		return -1;
	}
	//set SSID
	iret=system("iwconfig ra0 essid \"default\"");
	if (iret < 0)
	{
		return -1;
	}
	//set WEP key
	iret=system("iwconfig ra0 key off");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("iwconfig ra0 rate 54M");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("ifconfig ra0 \"192.168.2.1\"");
	if (iret < 0)
	{
		return -1;
	}
	iret = system("iwconfig ra0 channel 3");
	if (iret < 0)
	{
		return -1;
	}

	iret=system("ifconfig ra0 up");
	if (iret < 0)
	{
		return -1;
	}
	return 0;
}
int get_routers_list_xml(char * out)
{
	strcpy(out, ROUTERS_LIST_XML);
	return 0;
}

mlsErrorCode_t mlsWifi_SetupReset()
{
	int iret = 0,i;
	struct ifreq ifr;
	char buffer[128];
	mlsErrorCode_t retVal = MLS_SUCCESS;

	start_adhoc_wifi();
	iret=system("iwconfig ra0 mode managed");
	if (iret < 0)
	{
		return -1;
	}
	sleep(2);
	for(i =0; i < 3; i++)
	{
		iret=system("iwlist ra0 scanning > "ROUTERS_LIST_FILE);
		if (iret < 0)
		{
			return -1;
		}
		sleep(1);
		get_wifi_list(ROUTERS_LIST_FILE);
	}
	create_wifi_list_xmlfile(ROUTERS_LIST_XML);

	clean_wifi_list();

	iret=system("ifconfig ra0 down");
	if (iret < 0)
	{
		return -1;
	}
	iret=system("rmmod rt3070sta");
	if (iret < 0)
	{
		return -1;
	}
	

	printf("RT3070 goes to uAP mode...\n");
	system("sh /rt3070ap.sh");
	

	_mlsWifi_SetUAPConfigFile();

	printf("ifconfig ra0 up \n");

	iret = system("ifconfig ra0 "WIFI_Icam_IP_DEFAULT);
	if (iret < 0)
	{
		return MLS_ERROR;
	}

	iret = system("ifconfig ra0 up");
	if (iret < 0)
	{
		return MLS_ERROR;
	}
	

	getMacAddr(&ifr);
	sprintf(buffer,"iwpriv ra0 set SSID=\"Camera-%.2x%.2x%.2x\"",(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[3],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
						(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
	iret = system(buffer);
	if (iret < 0)
	{
		return MLS_ERROR;
	}
	
	if(checkWifiInterface() != MLS_SUCCESS){
		printf("Dectected Wifi module insmod faild\n");
		system("killall beep_arm");
		system("/mlsrb/beep_arm 20000 440 400 600 6000");
		sleep(8);
		system("killall beep_arm");
		exit_reboot();
	}
	

	iret = system("udhcpd /etc/udhcpd.conf");
	if (iret < 0)
	{
		return MLS_ERROR;
	}

	system("dnsd &");
	if (iret < 0)
	{
		return MLS_ERROR;
	}


	mlsioSetBlinkDuration(250,250);
	retVal = mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);

	return retVal;
}


mlsErrorCode_t _mlsWifi_infra_getRouter(icamWifiConfig_st *icamWifiConfig)
{
	mlsErrorCode_t retVal=0;
    FILE *fp = NULL;
    Char line[500],strssid[256];
    Char *pdes;
    Bool flagRouter = False;
    Int32 router_search_time = 3;
    Int32 iret;
	printf("ifconfig ra0 up\n");
	iret=system("ifconfig ra0 up");
	if (iret < 0)
	{
		return MLS_ERROR;
	}
	printf("iwconfig ra0 mode managed\n");
	iret=system("iwconfig ra0 mode managed");
	if (iret < 0)
	{
		return MLS_ERROR;
	}
    return retVal;
}



mlsErrorCode_t _mlsWifi_infra_createConfFile(icamWifiConfig_st *icamWifiConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	FILE *fwifispl = NULL;
	Char cmdkey[100];
	Char lenKey;


	fwifispl = fopen(WIFI_CONFIGSUPPLIANCE_FILE,"w");
	if (fwifispl == NULL)
	{

		retVal = MLS_ERR_OPENFILE;
		return retVal;
	}

	fprintf(fwifispl,"%s",WIFI_CONFIGSPL_HEADER);
	fprintf(fwifispl,"\tssid=\"%s\"\r\n",icamWifiConfig->strSSID);
	fprintf(fwifispl,"\tscan_ssid=1\r\n");
	switch (icamWifiConfig->authMode)
	{
		case wa_shared:
			fprintf(fwifispl,"%s","\tauth_alg=SHARED\r\n");
			fprintf(fwifispl,"%s","\tkey_mgmt=NONE\r\n");

			if (icamWifiConfig->encryptProtocol == we_wep)
			{
				lenKey = strlen(icamWifiConfig->strKey);
				if ((lenKey == 5)||(lenKey == 13))//ascii key
				{
					fprintf(fwifispl,"\twep_key%d=\"%s\"\r\n",icamWifiConfig->keyIndex,icamWifiConfig->strKey);
				}
				else //10 26
				{
					fprintf(fwifispl,"\twep_key%d=%s\r\n",icamWifiConfig->keyIndex,icamWifiConfig->strKey);
				}
				fprintf(fwifispl,"\twep_tx_keyidx=%d\r\n",icamWifiConfig->keyIndex);
			}
			break;
		case wa_open:
			fprintf(fwifispl,"%s","\tauth_alg=OPEN\r\n");
			fprintf(fwifispl,"%s","\tkey_mgmt=NONE\r\n");
			if (icamWifiConfig->encryptProtocol == we_wep)
			{
				lenKey = strlen(icamWifiConfig->strKey);
				if ((lenKey == 5)||(lenKey == 13))//ascii key
				{
					fprintf(fwifispl,"\twep_key%d=\"%s\"\r\n",icamWifiConfig->keyIndex,icamWifiConfig->strKey);
				}
				else //10 26
				{
					fprintf(fwifispl,"\twep_key%d=%s\r\n",icamWifiConfig->keyIndex,icamWifiConfig->strKey);
				}
				fprintf(fwifispl,"\twep_tx_keyidx=%d\r\n",icamWifiConfig->keyIndex);
			}
			break;
		case wa_wpa:
			sprintf(cmdkey,"\tpsk=\"%s\"\r\n",icamWifiConfig->strKey);
			fprintf(fwifispl,"%s",cmdkey);
			break;
		case wa_wpa2:
			fprintf(fwifispl,"%s","\tauth_alg=OPEN\r\n");
			fprintf(fwifispl,"%s","\tkey_mgmt=WPA-PSK\r\n");
			fprintf(fwifispl,"%s","\tproto=RSN\r\n");
			sprintf(cmdkey,"\tpsk=\"%s\"\r\n",icamWifiConfig->strKey);
			fprintf(fwifispl,"%s",cmdkey);
			break;
		default:
			break;
	}

	fprintf(fwifispl,"%s","\tpriority=2\r\n");
	fprintf(fwifispl,"%s",WIFI_CONFIGSPL_TAIL);


	fclose(fwifispl);
	return retVal;
}


mlsErrorCode_t _mlsWifi_infra_runShellCommand(icamWifiConfig_st *icamWifiConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Char buffer[256];
	Char retrytime = 20;//try to connect 5 times 20110905
	Int32 iret;

	iret=system("iwconfig ra0 mode managed");
	if (iret < 0)
	{
		return MLS_ERROR;
	}


	strcpy(buffer,"iwconfig ra0 essid \"");
	strcat(buffer,icamWifiConfig->strSSID);
	strcat(buffer,"\"");
	iret=system(buffer);
	if (iret < 0)
	{
		return MLS_ERROR;
	}
	iret=system("iwconfig ra0 rate 54M");
	if (iret < 0)
	{
		return MLS_ERROR;
	}
	iret=system("ifconfig ra0 up");
	if (iret < 0)
	{
		return MLS_ERROR;
	}

	system("mknod -m 644 /dev/urandom c 1 9");
	system("killall -KILL wpa_supplicant");
	printf("running wpa_supplicant as daemon in background...\n");
	system("wpa_supplicant -B -ira0 -c"WIFI_CONFIGSUPPLIANCE_FILE);
	sleep(5);

	system("wpa_cli -B -p /var/run/wpa_supplicant -ira0 -a/mlsrb/wpa_cli-action.sh");

	while(1)//retrytime>0)
	{
		if (icamWifiConfig->ipMode == ip_dhcp)
		{

			settingConfig_st flashInfor;
			char cmd[1024] = {'\0'};
			struct ifreq ifr;
			getMacAddr(&ifr);
			mlsGetConfig(&flashInfor);
			sprintf(cmd,"udhcpc -i ra0 -n -t 10 -S -s /etc/dhcp.script --hostname=\"%s%02X%02X\"",flashInfor.cameraName,
								(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[4],
									(unsigned char)ifr.ifr_ifru.ifru_hwaddr.sa_data[5]);
			iret = system(cmd);
			printf("udhcpc return %d\n",iret);
			if (iret != 0)//dhcp fail
			{
				printf("failed to get IP\n");
				retVal = MLS_ERR_WIFI_DHCPIP;
				retrytime--;
			}
			else
			{
				retVal = MLS_SUCCESS;
				printf("dhcpclient OK!\n");
				break;
			}
		}
		else if (icamWifiConfig->ipMode == ip_static)
		{


			/*1 Set ip and subnet mask */
			sprintf(buffer,"ifconfig ra0 %s netmask %s up",
						icamWifiConfig->ipAddressStatic,icamWifiConfig->ipSubnetMask);
			iret = system(buffer);
			if (iret < 0)
			{
				return MLS_ERROR;
			}

			if (strlen(icamWifiConfig->ipDefaultGateway) != 0)
			{

				//Set default gateway
				sprintf(buffer,"route add default gateway %s dev ra0",icamWifiConfig->ipDefaultGateway);
				system(buffer);

				//create resolv.conf file for nameserver to default gateway
				sprintf(buffer,"echo \"nameserver %s\" >  /etc/resolv.conf",icamWifiConfig->ipDefaultGateway);
				system(buffer);
				sprintf(buffer,"echo \"%s\" > /tmp/iprouter.txt",icamWifiConfig->ipDefaultGateway);
				system(buffer);
			}
			else
			{

			}

			retVal = _mlsWifi_infra_checkconnectivity(icamWifiConfig);
			if (retVal != MLS_SUCCESS)
			{

				sleep(5);
				retrytime--;
			}
			else
			{
				retVal = MLS_SUCCESS;

				
				break;
			}
		}

	}

	if (retVal == MLS_SUCCESS)
	{

	}

	return retVal;
}

int __try_connnect_to_anipaddr(char *destAddr)
{
    char cmd[100];
    char line[500];
    int ret = 0;
    FILE *fp;
    char *pdest;
    int send, recv;

    memset(cmd, 0, 100);
    sprintf(cmd, "ping %s -w 1 -c 2 > /tmp/pingResult%s", destAddr, destAddr);

    system(cmd);

    memset(cmd, 0, 100);
    sprintf(cmd, "/tmp/pingResult%s", destAddr);
    fp = fopen(cmd, "r");
    if( fp == NULL )
    {
        printf("Error, cannot open ping results\n");
        return 0;
    }
    else
    {

    }

    ret = 0;
    while(!feof(fp))
    {
        fgets(line, 500, fp);
        pdest = strstr(line, "packets transmitted");
        if(pdest != NULL)
        {
            send = line[0] - '0';
            recv = line[23] - '0';

            if (recv)
            {

                ret = 1;
                break;
            }
            else
            {

            }
        }
    }
    fclose(fp);
    return ret;
}


mlsErrorCode_t _mlsWifi_infra_checkconnectivity(icamWifiConfig_st *icamWifiConfig)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
//	FILE *fwfdhcp = NULL;
//	Char line[500];
//	Char ipserver[100];
	Int32 iret;

#if 1
	switch (icamWifiConfig->ipMode)
	{

		case ip_static:

			iret = __try_connnect_to_anipaddr(icamWifiConfig->ipDefaultGateway);
			if (iret == 0)
			{
				retVal = MLS_ERR_WIFI_NOTCONNECTIVITY;
			}
			else if(iret == 1)
			{
				retVal = MLS_SUCCESS;
			};

			break;
		default:
			break;
	}


#endif
	return retVal;
}


mlsErrorCode_t mlsWifi_SetupNormal(networkMode_t *networkMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	icamWifiConfig_st *icamWifiConfig;
	char buffer[256];


	retVal = GetIcamWifiConfig(&icamWifiConfig);
	if (icamWifiConfig->networkMode == wm_uAP)
	{
		return MLS_ERR_WIFI_WRONGCRC;
	}


	system("sh /rt3070sta.sh");
	
	if(checkWifiInterface() != MLS_SUCCESS){

		system("killall beep_arm");
		system("/mlsrb/beep_arm 20000 440 400 600 6000");
		sleep(8);
		system("killall beep_arm");
		exit_reboot();
	}
	
	retVal = mlsioSetSetupLed(IN_CMD_LED_EAR_ON);
	if(retVal != MLS_SUCCESS)
	{
		printf("set blink led error\n");
		return retVal;
	}


	if (icamWifiConfig->networkMode == wm_adhoc)
	{
		int iret;


		iret=system("iwconfig ra0 mode "WIFI_MODE_DEFAULT);
		if (iret < 0)
		{
			return MLS_ERROR;
		}
		//set SSID
		strcpy(buffer,"iwconfig ra0 essid \"");
		strcat(buffer,icamWifiConfig->strSSID);
		strcat(buffer,"\"");
		iret=system(buffer);
		if (iret < 0)
		{
			return MLS_ERROR;
		}
		//set WEP key
		if (strlen(icamWifiConfig->strKey) > 0)
		{
			strcpy(buffer,"iwconfig ra0 key ");
			strcat(buffer,icamWifiConfig->strKey);
			iret=system(buffer);
			if (iret < 0)
			{
				return MLS_ERROR;
			}
		}
		else
		{
			iret=system("iwconfig ra0 key off");
			if (iret < 0)
			{
				return MLS_ERROR;
			}
		}

		iret=system("iwconfig ra0 rate 54M");
		if (iret < 0)
		{
			return MLS_ERROR;
		}

		iret=system("ifconfig ra0 "WIFI_Icam_IP_DEFAULT);
		if (iret < 0)
		{
			return MLS_ERROR;
		}

		if ((1<=icamWifiConfig->channelAdhoc)&&(icamWifiConfig->channelAdhoc<=14))
		{

			sprintf(buffer,"iwconfig ra0 channel %d",icamWifiConfig->channelAdhoc);
			iret = system(buffer);
			if (iret < 0)
			{
				return MLS_ERROR;
			}
		}
		else
		{

		}

		iret=system("ifconfig ra0 up");
		if (iret < 0)
		{
			return MLS_ERROR;
		}
		*networkMode = wm_adhoc;
		mlsioSetBlinkDuration(250,250);
		mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);
	}
	else if (icamWifiConfig->networkMode == wm_infra)
	{

    	mlsioSetBlinkDuration(1000,2000);
    	mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);

        retVal = _mlsWifi_infra_getRouter(icamWifiConfig);
        if (retVal != MLS_SUCCESS)
        {

        	return retVal;
        }


        retVal = _mlsWifi_infra_createConfFile(icamWifiConfig);
        if (retVal != MLS_SUCCESS)
        {

        	return retVal;
        }


        retVal = _mlsWifi_infra_runShellCommand(icamWifiConfig);
        if (retVal != MLS_SUCCESS)
        {
        	printf("shell command error \n");
        	return retVal;
        }
    	mlsioSetSetupLed(IN_CMD_LED_EAR_ON);

	{
	
	}


        sprintf(buffer,"./ip_server --listen 10000 --reply 10001 --signature \"Mot-Cam\" --version %s --name \"%s\" --debug --working %s &",
        		GetVersionString(),
        		WIFI_SSIDNAME_DEFAULT,icamWifiConfig->workPort);

        system(buffer);

        *networkMode = wm_infra;

	}

	return retVal;
}



mlsErrorCode_t mlsWifi_Bootswitch()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	Int32 iret;
	FILE* fboot_option;

	fboot_option = fopen(BOOT_OPTION_DEV_NAME,"w");
	boot_opition_st boot_option;
	if (fboot_option == NULL)
	{
		printf("File open error\n");
		retVal = MLS_ERR_OPENFILE;
		return retVal;
	}
	memset(boot_option.swith_option_string,0,\
			sizeof(boot_option.swith_option_string));
	strcpy(boot_option.swith_option_string,BOOT_MAGIC_DATA_STR);

	boot_option.crc = nxcCRC32Compute(&boot_option,\
			sizeof(boot_option)-sizeof(boot_option.crc));
	iret = fwrite(&boot_option,sizeof(boot_opition_st),1,fboot_option);
	if (iret != 1)
	{
		printf("Wifi write file error! %d\n",iret);
		retVal = MLS_ERROR;
	}

	fclose(fboot_option);
	return retVal;
}

void *mlsCheckConRouterThread(void *arg)
{
	int connect = CONNECTED;
	FILE *f_ipServer = NULL;
	time_t start_counter,current_counter;
	start_counter = INT_MAX;
	current_counter = 0;
	while(!mlsioGetLedConfStop() && f_ipServer == NULL)
	{
		f_ipServer = fopen("/tmp/iprouter.txt","r");
		if(f_ipServer == NULL) sleep(1);
	}
	char ip[127] = {'\0'};
	fscanf(f_ipServer,"%s",ip);
	while(!mlsioGetLedConfStop())
	{
		int i = 0;
		if (mlsIcam_IsConnectToInternet())
		{

		    i=0;    
		}
		else
		{
		    for(i = 0; i < 5; i++)
		    {
			if(__try_connnect_to_anipaddr(ip) > 0) break;
			else sleep(2);
		    }
		}
		current_counter = time(NULL);
		if(i >=5)
		{
			if(connect != DISCONNECT){
				mlsioSetBlinkDuration(1000,1000);
				mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);
				connect = DISCONNECT;
				start_counter = time(NULL);
			}
			if (((current_counter - start_counter) > 5*60) && (connect == DISCONNECT))
			{

				mlsRapit_WdtRestart(1);
				sleep(100);
			}
		}
		else
		{
			if( connect != CONNECTED){
				mlsioSetSetupLed(IN_CMD_LED_EAR_ON);
				connect = CONNECTED;
				if ((current_counter - start_counter) > 60) // 30s
				{
				    system("killall -USR2 UPNP_client");
				}
			}
		}
		sleep(TIME_DELAY);
	}

	fclose(f_ipServer);
	pthread_exit(0);
}

mlsErrorCode_t mlsWifi_Setup(icamMode_t *icamMode,networkMode_t *networkMode)
{
	mlsErrorCode_t retVal = MLS_SUCCESS;


    FILE *fp = NULL;
    char line[500];
    char *pdes;


	usleep(300000); 

	if (*icamMode == icamModeReset)
	{

		retVal = mlsWifi_SetupReset();
		*networkMode = wm_adhoc;

	}
	else if (*icamMode == icamModeNormal)
	{
LOOP:

		retVal = mlsWifi_SetupNormal(networkMode);
		
		if(retVal != MLS_SUCCESS)
		{
			if (retVal == MLS_ERR_WIFI_WRONGCRC)
			{

				mlsioSetBlinkDuration(250,250);
				mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);
				goto LOOP;

			}
			else
			{
				mlsioSetBlinkDuration(250,250);
				mlsioSetSetupLed(IN_CMD_LED_EAR_BLINK);
				sleep(20);

				goto LOOP;
			}
		}
		else
		{

			int thr_id = -1;
			pthread_t p_thread;
			thr_id = pthread_create(&p_thread,NULL,mlsCheckConRouterThread,NULL);
			if(thr_id < 0)
			{
				perror("create thread to set time blink error");
				retVal = MLS_ERROR;
			}
			{
			    // running Bonjour
			    char tmpstr[256];
			    char IPAddr[64];
			    char Mac[32];
			    getLocalIPAddress(IPAddr);
			    mlsAuthen_ReadMACAddress(Mac,NULL);
			    system("killall -KILL mdnsd");
			    sprintf(tmpstr,"sysctl -w kernel.hostname=Camera-%s",Mac);
			    system(tmpstr);
			    sprintf(tmpstr,"/mlsrb/mdnsd ra0&");

			    system(tmpstr);
			}
		}
	}

    if (retVal != MLS_SUCCESS)
    {

    	return retVal;
    }


	//mls@dev03 check interface after setup rt3070
	/* check wifi interface - ra0 - is ok  in /proc/net/wireless */
    fp = fopen("/proc/net/wireless", "r");
    if (fp == NULL)
    {

        return MLS_ERR_OPENFILE;
    }

    retVal = MLS_ERR_WIFI_NOINTERFACE;
    //find wireless network interface
    while(!feof(fp))
    {
        fgets(line, 500, fp);

		pdes = strstr(line, "ra0");
		if(pdes == NULL)
		{
			continue;//return while loop to read new line
		}
		else
		{

			retVal = MLS_SUCCESS; //find the router having the same ssid
			break;
		}
    }

    fclose(fp);

    if (retVal != MLS_SUCCESS)
    {

    	return retVal;
    }

	return retVal;
}

mlsErrorCode_t mlsWifi_DeInit()
{
	mlsErrorCode_t retVal = MLS_SUCCESS;
	

	system("killall -q wpa_supplicant");

	return retVal;

}

//////////////////////////////////////////////////////////////////////////////////////////////////


mlsErrorCode_t mlsWifi_SetSSID(char *ssid)
{

	if(ssid == NULL || strlen(ssid) == 0){
		return MLS_ERROR;
	}
	FILE *f = fopen(WifiConfFile,"a");
	if(f == NULL){
		return MLS_ERROR;
	}
	fprintf(f,"SSID=%s\n",ssid);
	fclose(f);
	return MLS_SUCCESS;
}




int Mac_calculate_code (char* macaddress, char* userinput)
{
    static unsigned int mnttable[]={7,3,7,3,7,1};
    //char macaddress[13];
    char result[32];
    char tmp[3];
    unsigned short a1,a2,a3,a4,a5,a6;
    unsigned char a,b,c,d,e,f;
    tmp[2]=0;
    tmp[0]=macaddress[0];
    tmp[1]=macaddress[1];
    a6 = strtoul(tmp,NULL,16);
    tmp[0]=macaddress[2];
    tmp[1]=macaddress[3];
    a5 = strtoul(tmp,NULL,16);
    tmp[0]=macaddress[4];
    tmp[1]=macaddress[5];
    a4 = strtoul(tmp,NULL,16);
    tmp[0]=macaddress[6];
    tmp[1]=macaddress[7];
    a3 = strtoul(tmp,NULL,16);
    tmp[0]=macaddress[8];
    tmp[1]=macaddress[9];
    a2 = strtoul(tmp,NULL,16);
    tmp[0]=macaddress[10];
    tmp[1]=macaddress[11];
    a1 = strtoul(tmp,NULL,16);
    a = (unsigned char)(((a1+a5) * mnttable[0]) % 16) + 'a' ;
    b = (unsigned char)(((a2+a5) * mnttable[1]) % 16) + 'a' ;
    c = (unsigned char)(((a3+a5) * mnttable[2]) % 16) + 'a' ;
    d = (unsigned char)(((a4+a5) * mnttable[3]) % 16) + 'a' ;
    e = (unsigned char)(((a6+a5) * mnttable[4]) % 16) + 'a' ;
    f = (unsigned char)(((a3+a4) * mnttable[5]) % 16) + 'a' ;
    sprintf(result,"%c%c%c%c%c%c",a,b,c,d,e,f);
    if (strncmp(result,userinput,6) == 0)
    {
	return 0;
    }
    else
    {
	return 1;
    }
    
    return 0;
}


int getLocalIPAddress(char* IPStr) {
		int ret = -1;
        FILE * fp = popen("ifconfig ra0", "r");
        if (fp) {
                char *p=NULL, *e,*tmpptr; 
				size_t n;
                while ((getline(&p, &n, fp) > 0) && p) {
                   //printf("p = %08X\n",p);
                	tmpptr = strstr(p, "inet addr:");
                   if (tmpptr != NULL) {
                        tmpptr+=10;
                        e = strchr(tmpptr, ' ');
                        if (e != NULL) {
                             *e='\0';
                             strcpy(IPStr,tmpptr);
					//		 free (p);
							 ret = 0;
							 break;
                        }
                   }
                   //printf("12345");
		    //free(p);
                }
                free(p);
        }
        pclose(fp);
        return ret;
}



//////////////////////////////////////////////////////////////////////////////////////////////////
