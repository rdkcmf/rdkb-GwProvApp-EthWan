/* 
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/*#######################################################################
#   Copyright [2015] [ARRIS Corporation]
#
#   Licensed under the Apache License, Version 2.0 (the \"License\");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an \"AS IS\" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#######################################################################*/

#ifndef _GW_PROV_ETHWAN_SM_C_
#define _GW_PROV_ETHWAN_SM_C_

/*! \file gw_prov_ethwan_sm.c
    \brief gw epon provisioning
*/

/**************************************************************************/
/*      INCLUDES:                                                         */
/**************************************************************************/
#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if !defined(_PLATFORM_IPQ)
#include <sys/types.h>
#include <ruli.h>
#endif
#include <unistd.h>
#include <sysevent/sysevent.h>
#include <syscfg/syscfg.h>
#include <pthread.h>
#include "stdbool.h"
#include "gw_prov_ethwan.h"
#include "ccsp_hal_ethsw.h"

#ifdef FEATURE_SUPPORT_RDKLOG
#include "rdk_debug.h"
#endif

/* Global Variables*/
//char log_buff[1024];

/**************************************************************************/
/*      LOCAL VARIABLES:                                                  */
/**************************************************************************/
static int sysevent_fd;
static token_t sysevent_token;
static int sysevent_fd_gs;
static token_t sysevent_token_gs;
static pthread_t sysevent_tid;
#if defined(_PLATFORM_IPQ_)
static pthread_t linkstate_tid;
#endif
static uint32_t cb_registration_cnt;

#define INFO  0
#define WARNING  1
#define ERROR 2

#define DEBUG_INI_NAME  "/etc/debug.ini"
#define COMP_NAME "LOG.RDK.GWPROVETHWAN"
#define LOG_INFO 4
#define ER_NETDEVNAME "erouter0"
#define NETUTILS_IPv6_GLOBAL_ADDR_LEN             128


/* Syscfg keys used for calculating mac addresses of local interfaces and bridges */
#define BASE_MAC_SYSCFG_KEY                  "base_mac_address"
/* Offset at which LAN bridge mac addresses will start */
#define BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY    "base_mac_bridge_offset"
#define BASE_MAC_BRIDGE_OFFSET               0
/* Offset at which wired LAN mac addresses will start */
#define BASE_MAC_LAN_OFFSET_SYSCFG_KEY       "base_mac_lan_offset"
#define BASE_MAC_LAN_OFFSET                  129
/* Offset at which WiFi AP mac addresses will start */
#define BASE_MAC_WLAN_OFFSET_SYSCFG_KEY      "base_mac_wlan_offset"
#define BASE_MAC_WLAN_OFFSET                 145

#define BRG_INST_SIZE 5
#define BUF_SIZE 256
#ifdef FEATURE_SUPPORT_RDKLOG
#define GWPROVETHWANLOG(fmt ...)    {\
                                    char log_buff[1024];\
				    				snprintf(log_buff, 1023, fmt);\
                                    RDK_LOG(LOG_INFO, COMP_NAME, "%s", log_buff);\
                                    fflush(stdout);\
                                 }
#else
#define GWPROVETHWANLOG printf
#endif

//Note: By Moving this headerfile inclusion to "INCLUDES:" block, may run into build issues
#include "mso_mgmt_hal.h"

#if 0
#define IF_WANBRIDGE "erouter0"
#endif

#define _DEBUG 1
#define THREAD_NAME_LEN 16 //length is restricted to 16 characters, including the terminating null byte

/* Maximum callback registrations supported is 32 */
#define CB_REG_CNT_MAX 32

static int pnm_inited = 0;
static int netids_inited = 0;
static int webui_started = 0;
static int once = 0;

static void check_lan_wan_ready();
static void LAN_start();
static int hotspot_started = 0;
static appCallBack *obj_l[CB_REG_CNT_MAX];

/**************************************************************************/
/*! \fn int STATUS GWPEthWan_SyseventGetStr
 **************************************************************************
 *  \brief Set sysevent string Value
 *  \return 0:success, <0: failure
 **************************************************************************/

int GWPEthWan_SyseventGetStr(const char *name, unsigned char *out_value, int outbufsz)
{
  //Need to Implement

   return 0;		
}
#if defined(_PLATFORM_IPQ_)
/**************************************************************************/
/*! \fn int STATUS GWP_GetEthWanLinkStatus
 **************************************************************************
 *  \brief Get Wan Link Status
 *  \return 0:Link down, 1: Link up, <0: failure, errno. 
 **************************************************************************/
int GWP_GetEthWanLinkStatus()
{
	char buf[8] = {0};

	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
	GWPROVETHWANLOG("Info: Entry %s \n", __FUNCTION__);

	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\nGWP_GetEthWanLinkStatus\n");
	GWPROVETHWANLOG("\n**************************\n\n");
	
	return 0;	

	sysevent_get(sysevent_fd_gs, sysevent_token_gs, "phylink_wan_state", buf, sizeof(buf));

	if (!strcmp(buf, "up")) {
		return 1;
	}
	else if (!strcmp(buf, "down")) {
		return 0;
	}

	/* We should not reach here */
	GWPROVETHWANLOG("Error: Invalid value for event \'phylink_wan_state\' %s \n", __FUNCTION__);
	return -EINVAL;
}
#endif
static int GWP_EthWanLinkDown_callback()
{
	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
        system("sysevent set phylink_wan_state down");
	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\n GWP_EthWanLinkDown_callback \n");
	GWPROVETHWANLOG("\n**************************\n\n");
	GWPROVETHWANLOG(" Stopping wan service\n");
        system("sysevent set wan-stop");
	system("ifconfig erouter0 up");
	return 0;
}
#if !defined(_PLATFORM_IPQ_)
static int ethGetPHYRate
    (
        CCSP_HAL_ETHSW_PORT PortId
    )
{
    INT status                              = RETURN_ERR;
    CCSP_HAL_ETHSW_LINK_RATE LinkRate       = CCSP_HAL_ETHSW_LINK_NULL;
    CCSP_HAL_ETHSW_DUPLEX_MODE DuplexMode   = CCSP_HAL_ETHSW_DUPLEX_Auto;
    INT PHYRate                             = 0;

    status = CcspHalEthSwGetPortCfg(PortId, &LinkRate, &DuplexMode);
    if (RETURN_OK == status)
    {
     	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
        switch (LinkRate)
        {
            case CCSP_HAL_ETHSW_LINK_10Mbps:
            {
                PHYRate = 10;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_100Mbps:
            {
                PHYRate = 100;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_1Gbps:
            {
                PHYRate = 1000;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_10Gbps:
            {
                PHYRate = 10000;
                break;
            }
            case CCSP_HAL_ETHSW_LINK_Auto:
            {
	        GWPROVETHWANLOG(" Entry %s LINK_Auto \n", __FUNCTION__);
                PHYRate = 1000;
                break;
            }
            default:
            {
                PHYRate = 0;
                break;
            }
        }
    }
    return PHYRate;
}
#endif
static int GWP_EthWanLinkUp_callback()
{
	GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
	system("sysevent set phylink_wan_state up");
	GWPROVETHWANLOG("\n**************************\n");
	GWPROVETHWANLOG("\nGWP_EthWanLinkUp_callback\n");
	GWPROVETHWANLOG("\n**************************\n\n");
	system("sysevent set bridge_mode 0"); // to boot in router mode
        char wanPhyName[20];
        char out_value[20];
        int outbufsz = sizeof(out_value);

        if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
        {
           strcpy(wanPhyName, out_value);
           printf("wanPhyName = %s\n", wanPhyName);
        }
        else
        {
           return -1;
        }

        printf("Starting wan service\n");
        GWPROVETHWANLOG(" Starting wan service\n");
        system("sysevent set wan-start");
        int i = 1;
	    system("sysevent set ethwan-initialized 1");
		system("syscfg set eth_wan_enabled true"); // to handle Factory reset case
		system("syscfg commit");
#if !defined(_PLATFORM_IPQ_)
        GWPROVETHWANLOG("WAN_MODE: Ethernet %d\n", ethGetPHYRate(i));
#endif
        return 0;
}

#if defined(_PLATFORM_IPQ_)
/**************************************************************************/
/*! \fn void *GWP_linkstate_threadfunc(void *)
 **************************************************************************
 *  \brief Thread function to check the link state
 *  \return
 **************************************************************************/
static void *GWPEthWan_linkstate_threadfunc(void *data)
{
    char *temp;
    char command[50] = {0};
    char wanPhyName[20] = {0};
    char out_value[20] = {0};
    int outbufsz = sizeof(out_value);
    int is_mode_switch = 0;
    int is_wan_restart = 0;
    char buf[8] = {0};
    int i;

    char* buff = NULL;
    buff = malloc(sizeof(char)*50);
    if(buff == NULL)
    {
        return -1;
    }
    char previousLinkStatus[10] = "down";
    if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
        strcpy(wanPhyName, out_value);
        printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
        if(buff != NULL)
            free(buff);
        return -1;
    }
    sprintf(command, "cat /sys/class/net/%s/operstate", wanPhyName);

    while(1)
    {
        FILE *fp;
        memset(buff,0,sizeof(buff));

       /*
        * utopia_restart values :
        *      0 : mode switch is not in progress
        *      1 : mode switch is in progress
        */
       sysevent_get(sysevent_fd_gs, sysevent_token_gs, "utopia_restart", buf, sizeof(buf));
       is_mode_switch = atoi(buf);
       memset(buf, 0, sizeof(buf));
       sysevent_get(sysevent_fd_gs, sysevent_token_gs, "wan-restarting", buf, sizeof(buf));
       is_wan_restart = atoi(buf);

       if ( !( is_mode_switch || is_wan_restart) )
       {
        /* Open the command for reading. */
        fp = popen(command, "r");
        if (fp == NULL)
        {
            printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
            continue;
        }

        /* Read the output a line at a time - output it. */
        while (fgets(buff, 50, fp) != NULL)
        {
            /*printf("Ethernet status :%s", buff);*/
            temp = strchr(buff, '\n');
            if(temp)
                *temp = '\0';
        }

        /* close */
        pclose(fp);
        if(!strcmp(buff, (const char *)previousLinkStatus))
        {
            /*printf("Link status not changed\n");*/
        }
        else
        {
            if(!strcmp(buff, "up"))
            {
                /*printf("Ethernet status :%s\n", buff);*/
                GWP_EthWanLinkUp_callback();
#if defined(_PLATFORM_IPQ_)
		/* We call all the registered callbacks */
		for (i = 0; i < cb_registration_cnt; i++) {
			if (obj_l[i]->pGWP_act_EthWanLinkUP)
				obj_l[i]->pGWP_act_EthWanLinkUP();
		}
#endif

            }
            else if(!strcmp(buff, "down"))
            {
                /*printf("Ethernet status :%s\n", buff);*/
                GWP_EthWanLinkDown_callback();
#if defined(_PLATFORM_IPQ_)
		/* We call all the registered callbacks */
		for (i = 0; i < cb_registration_cnt; i++) {
			if (obj_l[i]->pGWP_act_EthWanLinkDown)
				obj_l[i]->pGWP_act_EthWanLinkDown();
		}
#endif

            }
            else
            {
                sleep(5);
                continue;
            }
            memset(previousLinkStatus,0,sizeof(previousLinkStatus));
            strcpy((char *)previousLinkStatus, buff);
            /*printf("Previous Ethernet status :%s\n", (char *)previousLinkStatus);*/
        }
       }
        sleep(5);
    }
    if(buff != NULL)
        free(buff);

    return 0;
}
#endif

/** 
* @brief  Create state machine for event handling from DOCSIS stack
*\n Prototype :
		int GWP_RegisterEthWan_Callback
	(
		appCallBack *pAppCallBack
	)
*\n Caller : int main(int argc, char *argv[])
*
*
* @param[in] appCallBack *pAppCallBack.
* @param[out] None.
* @retval 0: Success, <0: Failure,errno.
* 	     ENOTSUP: Registration count exceeds 32
* 	     EINVAL: NULL Function Pointers passed
*/
#if defined(_PLATFORM_IPQ_)
int GWP_RegisterEthWan_Callback(appCallBack *pAppCallBack)
 {
	GWPROVETHWANLOG("Info: Entering into %s\n",__FUNCTION__);

	/* We support maximum 32 callback Registrations, beyond which 
	 * the Registration fails*/
	if (cb_registration_cnt == CB_REG_CNT_MAX) {
		fprintf(stderr, "Error: %s Max callback registrations allowed is 32. \n",__FUNCTION__);
		return -ENOTSUP;
	}

	/* We return Failure if both callbacks are NULL */
	if ((!pAppCallBack->pGWP_act_EthWanLinkDown) && (!pAppCallBack->pGWP_act_EthWanLinkUP)) {
		fprintf(stderr, "Error: %s NULL function pointers passed as arg\n",__FUNCTION__);
		return -EINVAL;
	}

	obj_l[cb_registration_cnt]->pGWP_act_EthWanLinkDown =  pAppCallBack->pGWP_act_EthWanLinkDown;
	obj_l[cb_registration_cnt]->pGWP_act_EthWanLinkUP =  pAppCallBack->pGWP_act_EthWanLinkUP;

	cb_registration_cnt++;
 	return 0;
 }
#endif
static void check_lan_wan_ready()
{
        char lan_st[16];
        char wan_st[16];
        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "lan-status", lan_st, sizeof(lan_st));
        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "wan-status", wan_st, sizeof(wan_st));

        printf("****************************************************\n");
        printf("       %s   %s\n", lan_st, wan_st);
        printf("****************************************************\n");

	if (!strcmp(lan_st, "started") && !strcmp(wan_st, "started"))
        {
           sysevent_set(sysevent_fd_gs, sysevent_token_gs, "start-misc", "ready", 0);
           once = 1;
        }

	return;
}
/**************************************************************************/
/*! \fn void *GWPEthWan_sysevent_handler(void *data)
 **************************************************************************
 *  \brief Function to process sysevent event
 *  \return 0
**************************************************************************/
static void *GWPEthWan_sysevent_handler(void *data)
{
    GWPROVETHWANLOG( "Entering into %s\n",__FUNCTION__);
    async_id_t ipv4_status_asyncid;
    async_id_t ipv6_status_asyncid;
    async_id_t dhcp_server_status_asyncid;
    async_id_t firewall_restart_asyncid;
    async_id_t lan_restart_asyncid;
    async_id_t lan_stop_asyncid;
    async_id_t lan_status_asyncid;
    async_id_t pnm_status_asyncid;
    async_id_t primary_lan_l3net_asyncid;
    async_id_t homesecurity_lan_l3net_asyncid;
    int l2net_inst_up = FALSE;

    /* RIPD/Zebra event ids */
    async_id_t wan_status_asyncid;

    sysevent_set_options(sysevent_fd, sysevent_token, "ipv4-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv4-status",  &ipv4_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "ipv6-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "ipv6-status",  &ipv6_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "dhcp_server-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "dhcp_server-restart",  &dhcp_server_status_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "firewall-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "firewall-restart",  &firewall_restart_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "lan-restart", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-restart",  &lan_restart_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "lan-stop", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-stop",  &lan_stop_asyncid);

#if defined(_PLATFORM_IPQ_)
    sysevent_set_options(sysevent_fd, sysevent_token, "bring-lan", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "bring-lan",  &pnm_status_asyncid);
#else
    sysevent_set_options(sysevent_fd, sysevent_token, "pnm-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "pnm-status",&pnm_status_asyncid);
#endif
    sysevent_set_options(sysevent_fd, sysevent_token, "primary_lan_l3net", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "primary_lan_l3net",  &primary_lan_l3net_asyncid);

    sysevent_set_options(sysevent_fd, sysevent_token, "homesecurity_lan_l3net", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "homesecurity_lan_l3net",  &homesecurity_lan_l3net_asyncid);
    /* Route events to start ripd and zebra */
    sysevent_set_options    (sysevent_fd, sysevent_token, "lan-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "lan-status",  &lan_status_asyncid);
    sysevent_set_options    (sysevent_fd, sysevent_token, "wan-status", TUPLE_FLAG_EVENT);
    sysevent_setnotification(sysevent_fd, sysevent_token, "wan-status",  &wan_status_asyncid);

   for (;;)
   {
        unsigned char name[25], val[42],buf[BUF_SIZE];;
        int namelen = sizeof(name);
        int vallen  = sizeof(val);
        int err;
		int isNTPSyncd = FALSE;
        async_id_t getnotification_asyncid;
	    char brlan0_inst[BRG_INST_SIZE], brlan1_inst[BRG_INST_SIZE];
        char* l3net_inst = NULL;

        err = sysevent_getnotification(sysevent_fd, sysevent_token, name, &namelen,  val, &vallen, &getnotification_asyncid);

        if (err)
        {
           GWPROVETHWANLOG("sysevent_getnotification failed with error: %d\n", err);
        }
        else
        {
            GWPROVETHWANLOG("received notification event %s\n", name);
	    
            printf("received notification event %s\n", name);fflush(stdout);
	   if (strcmp(name, "ipv4-status")==0)
            {
                if (strcmp(val, "up")==0)
                {
                    //Need to Implement 
                }
                else if (strcmp(val, "down")==0)
                {
                    //Need to Implement 
                }
            }
            else if (strcmp(name, "ipv6-status")==0)
            {
                if (strcmp(val, "up")==0)
                {
                    //Need to Implement 
                }
                else
                {
                    //Need to Implement 
                }
            }

            else if (strcmp(name, "system-restart") == 0)
            {
                printf("gw_prov_sm: got system restart\n");
                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "utopia_restart", "1", 0);
            }

            else if ( (strcmp(name, "dhcp_server-restart")==0) || (strcmp(name, "dhcpv6s_server")==0) )
            {
               //Need to Implement 
            }
            else if (strcmp(name, "firewall-restart")==0)
            {
                //Need to Implement 
            }
#if defined(_PLATFORM_IPQ_)
            else if ((strcmp(name, "lan-status") == 0 ||
                     strcmp(name, "wan-status") == 0) &&
                     strcmp(val, "started") == 0)
#else
            else if ((strcmp(name, "wan-status") == 0) &&
                     (strcmp(val, "started") == 0))
#endif
            {
                int restartFirewall = 0;
                // When lan-status and wan-status started, only call functions when both are started
                // or bad things will happen
                do
                {
                    unsigned char lan_status[20];
                    unsigned char wan_status[20];
#if defined(_PLATFORM_IPQ_)
                    // Make sure lan-status is started first...
                   lan_status[0] = '\0';
                    if (GWPEthWan_SyseventGetStr("lan-status", lan_status, sizeof(lan_status)) < 0)
                    {
                        break;
                    }
                    
                    if (strcmp(lan_status, "started") != 0)
                    {
                       if (!webui_started) {
                           system("/bin/sh /etc/webgui.sh");
                           webui_started = 1;
                       }
                    }
#endif
                    
                    // Make sure wan-status is started second...
                    wan_status[0] = '\0';
                    if (GWPEthWan_SyseventGetStr("wan-status", wan_status, sizeof(wan_status)) < 0)
                    {
                        break;
                    }
                    
                    if (strcmp(wan_status, "started") != 0)
                    {
                        if (!once) {
                              check_lan_wan_ready();
                        }
						if (isNTPSyncd == FALSE)
						{
							isNTPSyncd = TRUE;
			        		//Need to sync device time
							system("systemctl stop ntpd.service");
							system("echo server ntp.ccp.xcal.tv > /tmp/ntp_tmp_sync.conf");
							system("ntpd -c /tmp/ntp_tmp_sync.conf -g -q");	 
							system("killall ntpd");	 
							system("systemctl start ntpd.service");
						}
                    }
		    system("touch /tmp/phylink_wan_state_up");
                    system("sysevent set sshd-restart");
                    restartFirewall = 1;
                } while (0);

                if (strcmp(name, "lan-status") == 0)
                {
                     //Need to Implement 
                }
                
                if (restartFirewall == 1)
                {
                    //Need to Implement 
                }
            }
            else if (strcmp(name, "lan-restart") == 0)
            {
                if (strcmp(val, "1")==0)
                {
                    //Need to Implement 
                }
            }
            else if (strcmp(name, "lan-stop") == 0)
            {
	        //Need to Implement 
            }
            else if (strcmp(name, "pnm-status") == 0)
            {
             	if (strcmp(val, "up")==0)
                 {
		         LAN_start();

                 }
            }
            else if (strcmp(name, "bring-lan") == 0)
            {
                if (strcmp(val, "up")==0)
                 {
                    pnm_inited = 1;
                    if (netids_inited) {
                        LAN_start();
                    }
                 }
            }
            else if (strcmp(name, "primary_lan_l3net") == 0)
            {
                GWPROVETHWANLOG(" primary_lan_l3net received \n");
                if (pnm_inited)
                 {
                    LAN_start();
                 }
                netids_inited = 1;
            }
            else if (strcmp(name, "homesecurity_lan_l3net") == 0)
            {
                GWPROVETHWANLOG(" homesecurity_lan_l3net received \n");
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", val, 0);

            }
            else if (strcmp(name, "tr_" ER_NETDEVNAME "_dhcpv6_client_v6addr") == 0)
	    {
                Uint8 v6addr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                Uint8 soladdr[ NETUTILS_IPv6_GLOBAL_ADDR_LEN / sizeof(Uint8) ];
                inet_pton(AF_INET6, val, v6addr);

                inet_ntop(AF_INET6, soladdr, val, sizeof(val));


                sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv6_"ER_NETDEVNAME"_dhcp_solicNodeAddr", val,0);

                unsigned char lan_wan_ready = 0;
                char command[256], result_buf[32];
                command[0] = result_buf[0] = '\0';

                sysevent_get(sysevent_fd_gs, sysevent_token_gs, "start-misc", result_buf, sizeof(result_buf));
                lan_wan_ready = strstr(result_buf, "ready") == NULL ? 0 : 1;

                if(!lan_wan_ready) {
                    snprintf(command, sizeof(command),"ip6tables -t mangle -I PREROUTING 1 -i %s -d %s -p ipv6-icmp -m icmp6 --icmpv6-type 135 -m limit --limit 20/sec -j ACCEPT", ER_NETDEVNAME, val);
                    system(command);
                }
                else
                    sysevent_set(sysevent_fd_gs, sysevent_token_gs, "firewall-restart", "",0);
            }
#if !defined(_PLATFORM_IPQ_)
            else if (strcmp(name, "lan-status") == 0 )
            {
		 GWPROVETHWANLOG(" lan-status received \n");
				if (strcmp(val, "started") == 0) {
				    if (!webui_started) { 
				        startWebUIProcess();
				        webui_started = 1 ;

				        //Piggy back off the webui start event to signal XHS startup
				        sysevent_get(sysevent_fd_gs, sysevent_token_gs, "homesecurity_lan_l3net", buf, sizeof(buf));
				        if (buf[0] != '\0') sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", buf, 0);

				    }
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "primary_lan_l3net", buf, sizeof(buf));
			strncpy(brlan0_inst, buf, BRG_INST_SIZE-1);
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "homesecurity_lan_l3net", buf, sizeof(buf));
			strncpy(brlan1_inst, buf, BRG_INST_SIZE-1);

			/*Get the active bridge instances and bring up the bridges */
			sysevent_get(sysevent_fd_gs, sysevent_token_gs, "l3net_instances", buf, sizeof(buf));
			
			l3net_inst = strtok(buf, " ");
			while(l3net_inst != NULL)
			{
			    /*brlan0 and brlan1 are already up. We should not call their instances again*/
			    if(!((strcmp(l3net_inst, brlan0_inst)==0) || (strcmp(l3net_inst, brlan1_inst)==0)))
			    {
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", l3net_inst, 0);
				l2net_inst_up = TRUE;
			    }
			    l3net_inst = strtok(NULL, " ");
			}
			if(l2net_inst_up == FALSE)
			{
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", brlan0_inst, 0);
				sysevent_set(sysevent_fd_gs, sysevent_token_gs, "ipv4-up", brlan1_inst, 0);
			}
				   
				    if (!hotspot_started) {
		#if !defined(INTEL_PUMA7) && !defined(_COSA_BCM_MIPS_) && !defined(_COSA_BCM_ARM_)
				        printf("Not Calling hotspot-start for XB3 it will be done in cosa_start_rem.sh\n");
		#else
				        /* TCXB6-1922: Adding 5 seconds delay as sysevents are not getting handled properly */
				        sleep(5);
				        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "hotspot-start", "", 0);
				        hotspot_started = 1 ;
		#endif
				    }
	    } 
	    }
#endif
            else
            {
               GWPROVETHWANLOG( "undefined event %s \n",name);
            }			
        }
    }

    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
}

#if defined(_PLATFORM_IPQ_)
static int GWP_act_ProvEntry_callback()
{
    GWPROVETHWANLOG( "Entering into %s\n",__FUNCTION__);

    system("mkdir -p /nvram");
    system("rm -f /nvram/dnsmasq.leases");
    system("syslogd -f /etc/syslog.conf");

    //copy files that are needed by CCSP modules
    system("cp /usr/ccsp/ccsp_msg.cfg /tmp");
    system("touch /tmp/cp_subsys_ert");

    /* Below link is created because crond is expecting /crontabs/ dir instead of /var/spool/cron/crontabs */
    system("ln -s /var/spool/cron/crontabs /");
    /* directory /var/run/firewall because crond is expecting this dir to execute time specific blocking of firewall*/
    system("mkdir -p /var/run/firewall");

    system("/etc/utopia/utopia_init.sh");

    syscfg_init();

    sleep(2);
    /*
     * Invoking board specific configuration script to set the board related
     * syscfg parameters.
     */
    system("/usr/bin/apply_board_defaults.sh");

    char command[50];
    char wanPhyName[20];
    char out_value[20];
    int outbufsz = sizeof(out_value);

    char previousLinkStatus[10] = "down";
    if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
       strcpy(wanPhyName, out_value);
       printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
       return -1;
    }

    system("ifconfig eth0 down");
    memset(command,0,sizeof(command));
    sprintf(command, "ip link set eth0 name %s", wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);

    memset(command,0,sizeof(command));
    sprintf(command, "ifconfig %s up", wanPhyName);
    printf("************************value of command = %s\***********************n", command);
    system(command);

    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan", &sysevent_token);

    if (sysevent_fd >= 0)
    {
        system("sysevent set phylink_wan_state down");
        printf(" Creating Thread  GWPEthWan_sysevent_threadfunc \n");
        GWPROVETHWANLOG(" Creating Thread  GWPEthWan_sysevent_threadfunc \n");
        pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
    }

    //Make another connection for gets/sets
    sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan-gs", &sysevent_token_gs);

    LAN_start();

    pthread_create(&linkstate_tid, NULL, GWPEthWan_linkstate_threadfunc, NULL);

    return 0;
}
#else
static int GWP_act_ProvEntry_callback()
{
    int i;
    int sysevent_bridge_mode = 0;
    char command[50];
    char wanPhyName[20];
    char out_value[20];
    int outbufsz = sizeof(out_value);
    GWPROVETHWANLOG(" Entry %s \n", __FUNCTION__);
    //system("sysevent set lan-start");
   
/* TODO: OEM to implement swctl apis */



    GWPROVETHWANLOG(" Calling /etc/utopia/utopia_init.sh \n"); 
    system("/etc/utopia/utopia_init.sh");

    syscfg_init();
    sleep(2);
    sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov", &sysevent_token);

    if (sysevent_fd >= 0)
    {
        system("sysevent set phylink_wan_state down");
        GWPROVETHWANLOG(" Creating Thread  GWPEthWan_sysevent_handler \n"); 
       // pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
    }
    
    //Make another connection for gets/sets
    sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov-gs", &sysevent_token_gs);

   if (!syscfg_get(NULL, "wan_physical_ifname", out_value, outbufsz))
    {
       strcpy(wanPhyName, out_value);
       printf("wanPhyName = %s\n", wanPhyName);
    }
    else
    {
        strcpy(wanPhyName, "erouter0");
      
    }


        macaddr_t macAddr;

        getWanMacAddress(&macAddr);
                printf("eRouter macAddr ");
                for (i=0;i<6;i++)
		{
                   printf("%2x ",macAddr.hw[i]);
		}
                printf(" \n");
    char wan_mac[18];// = {0};
    sprintf(wan_mac, "%02x:%02x:%02x:%02x:%02x:%02x",macAddr.hw[0],macAddr.hw[1],macAddr.hw[2],macAddr.hw[3],macAddr.hw[4],macAddr.hw[5]);
    system("ifconfig eth0 down");
    memset(command,0,sizeof(command));
    system("vlan_util del_interface brlan0 eth0");
#ifdef _COSA_BCM_ARM_
    sprintf(command, "ifconfig %s down; ip link set %s name cm0", wanPhyName,wanPhyName);
#else
    sprintf(command, "ifconfig %s down; ip link set %s name dummy-rf", wanPhyName,wanPhyName);
#endif
    system(command);
    memset(command,0,sizeof(command));
    #if 0
    sprintf(command, "ip link set eth0 name %s", wanPhyName);
    #else
        sprintf(command, "brctl addbr %s; brctl addif %s eth0", wanPhyName,wanPhyName);
    #endif
    printf("****************value of command = %s**********************\n", command);
    system(command);

#ifdef _COSA_BCM_ARM_
    system("ifconfig cm0 up");
    memset(command,0,sizeof(command));
    sprintf(command, "brctl addbr %s; brctl addif %s cm0", wanPhyName,wanPhyName);
    printf("****************value of command = %s**********************\n", command);
    system(command);
#endif

    memset(command,0,sizeof(command));
    sprintf(command, "ifconfig %s down", wanPhyName);
    system(command);
    memset(command,0,sizeof(command));
    sprintf(command, "ifconfig %s hw ether %s", wanPhyName,wan_mac);
    printf("************************value of command = %s\***********************n", command);
    system(command);
    memset(command,0,sizeof(command));
    sprintf(command, "sysevent set eth_wan_mac %s", wan_mac);
    system(command);
      // setNetworkDeviceMacAddress(ER_NETDEVNAME,&macAddr);
    memset(command,0,sizeof(command));
    sprintf(command, "ifconfig %s up", wanPhyName);
    printf("************************value of command = %s\***********************n", command);
    system(command);
                                


    GWP_EthWanLinkUp_callback();
    return 0;
}
#endif

static bool GWPEthWan_Register_sysevent()
{
    bool status = false;
    const int max_retries = 6;
    int retry = 0;
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    do
    {
        sysevent_fd = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan", &sysevent_token);
        if (sysevent_fd < 0)
        {
            GWPROVETHWANLOG("gw_prov_ethwan failed to register with sysevent daemon\n");
            status = false;
        }
        else
        {
            GWPROVETHWANLOG( "gw_prov_ethwan registered with sysevent daemon successfully\n");
            status = true;
        }
        
        //Make another connection for gets/sets
        sysevent_fd_gs = sysevent_open("127.0.0.1", SE_SERVER_WELL_KNOWN_PORT, SE_VERSION, "gw_prov_ethwan-gs", &sysevent_token_gs);
        if (sysevent_fd_gs < 0)
        {
            GWPROVETHWANLOG( "gw_prov_ethwan-gs failed to register with sysevent daemon\n");
            status = false;
        }
        else
        {
            GWPROVETHWANLOG("gw_prov_ethwan-gs registered with sysevent daemon successfully\n");
            status = true;
        }

    }while((status == false) && (retry++ < max_retries));

    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}

static int GWP_ETHWAN_Init()
{
    int status = 0;
    int thread_status = 0;
    char thread_name[THREAD_NAME_LEN];
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);

    if (GWPEthWan_Register_sysevent() == false)
    {
        GWPROVETHWANLOG("GWP_ETHWAN_Register_sysevent failed\n");
        status = -1;
    }
    else 
    {
        GWPROVETHWANLOG("GWP_ETHWAN_Register_sysevent Successful\n");
    
        thread_status = pthread_create(&sysevent_tid, NULL, GWPEthWan_sysevent_handler, NULL);
        if (thread_status == 0)
        {
            GWPROVETHWANLOG("GWP_ETHWAN_sysevent_handler thread created successfully\n");              
            sleep(5);
        }
        else
        {
            GWPROVETHWANLOG("%s error occured while creating GWP_ETHWAN_sysevent_handler thread\n", strerror(errno));
            status = -1;
        }
    }
    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}

static bool checkIfAlreadyRunning(const char* name)
{
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    bool status = true;

    FILE *fp = fopen("/tmp/.gwprovethwan.pid", "r");
    if (fp == NULL)
    {
        GWPROVETHWANLOG("File /tmp/.gwprovethwan.pid doesn't exist\n");
        FILE *pfp = fopen("/tmp/.gwprovethwan.pid", "w");
        if (pfp == NULL)
        {
            GWPROVETHWANLOG("Error in creating file /tmp/.gwprovethwan.pid\n");
        }
        else
        {
            pid_t pid = getpid();
            fprintf(pfp, "%d", pid);
            fclose(pfp);
        }
        status = false;
    }
    else
    {
        fclose(fp);
    }
    GWPROVETHWANLOG("Exiting from %s\n",__FUNCTION__);
    return status;
}


static void daemonize(void) 
{
    GWPROVETHWANLOG("Entering into %s\n",__FUNCTION__);
    int fd;
    switch (fork()) {
    case 0:
      	GWPROVETHWANLOG("In child pid=%d\n", getpid());
        break;
    case -1:
    	// Error
    	GWPROVETHWANLOG( "Error daemonizing (fork)! %d - %s\n", errno, strerror(errno));
    	exit(0);
    	break;
    default:
     	GWPROVETHWANLOG("In parent exiting\n");
    	_exit(0);
    }

    //create new session and process group
    if (setsid() < 0) {
        GWPROVETHWANLOG( "Error demonizing (setsid)! %d - %s\n", errno, strerror(errno));
    	exit(0);
    }    

#ifndef  _DEBUG
    //redirect fd's 0,1,2 to /dev/null     
    fd = open("/dev/null", O_RDONLY);
    if (fd != 0) {
        dup2(fd, 0);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 1) {
        dup2(fd, 1);
        close(fd);
    }
    fd = open("/dev/null", O_WRONLY);
    if (fd != 2) {
        dup2(fd, 2);
        close(fd);
    }
#endif
}

static void LAN_start() {
        GWPROVETHWANLOG("Utopia starting lan...\n");
        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "lan-start", "", 0);
        sysevent_set(sysevent_fd_gs, sysevent_token_gs, "dhcp_server-resync", "", 0);

        return;
}

/**************************************************************************/
/*! \fn int main(int argc, char *argv)
 **************************************************************************
 *  \brief Init and run the Provisioning process
 *  \param[in] argc
 *  \param[in] argv
 *  \return Currently, never exits
 **************************************************************************/
int main(int argc, char *argv[])
{
    int status = 0;
    const int max_retries = 6;
    int retry = 0;
    appCallBack *obj     =    NULL;
    char sysevent_cmd[80];
    int i;
    sleep(2);
#if !defined(_PLATFORM_IPQ_)
#ifdef FEATURE_SUPPORT_RDKLOG
    setenv("LOG4C_RCPATH","/rdklogger",1);
    rdk_logger_init(DEBUG_INI_NAME);
#endif
    GWPROVETHWANLOG("GWP_ETHWAN Started gw_prov_EthWan\n");


    if (checkIfAlreadyRunning(argv[0]) == true)
    {
        GWPROVETHWANLOG("Process %s already running\n", argv[0]);
        status = 1;
    }
    else
    {    


 			GWP_act_ProvEntry_callback();
#if !defined(_PLATFORM_IPQ_)
	        CcspHalEthSwInit();
#endif
            if (GWP_ETHWAN_Init() != 0)
            {
                GWPROVETHWANLOG( "GWP_ETHWAN Initialization failed\n");
                status = 1;
            }
            else
            {
                GWPROVETHWANLOG("GWP_ETHWAN initialization completed\n");
                //wait for sysevent_tid thread to terminate
                //pthread_join(sysevent_tid, NULL);
                
                GWPROVETHWANLOG("sysevent_tid thread terminated\n");
            }
    GWPROVETHWANLOG("GWP_ETHWAN initialization completed\n");
	obj = ( appCallBack* ) malloc ( sizeof ( appCallBack ) );

	obj->pGWP_act_EthWanLinkDown =  GWP_EthWanLinkDown_callback;
	obj->pGWP_act_EthWanLinkUP =  GWP_EthWanLinkUp_callback;
	GWPROVETHWANLOG("GWP_ETHWAN Creating RegisterEthWan Handler\n");
	GWP_RegisterEthWan_Callback ( obj );
	GWPROVETHWANLOG("GWP_ETHWAN Creating RegisterEthWan Handler over\n");

    if ((syscfg_set(NULL, BASE_MAC_SYSCFG_KEY, sysevent_cmd) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_SYSCFG_KEY);
    }

    /* Update LAN bridge mac address offset */
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "%d", BASE_MAC_BRIDGE_OFFSET);
    if ((syscfg_set(NULL, BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY, sysevent_cmd) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_BRIDGE_OFFSET_SYSCFG_KEY);
    }

    /* Update wired LAN interface mac address offset */
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "%d", BASE_MAC_LAN_OFFSET);
    if ((syscfg_set(NULL, BASE_MAC_LAN_OFFSET_SYSCFG_KEY, sysevent_cmd) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_LAN_OFFSET_SYSCFG_KEY);
    }

    /* Update WiFi interface mac address offset */
    snprintf(sysevent_cmd, sizeof(sysevent_cmd), "%d", BASE_MAC_WLAN_OFFSET);
    if ((syscfg_set(NULL, BASE_MAC_WLAN_OFFSET_SYSCFG_KEY, sysevent_cmd) != 0))
    {
        fprintf(stderr, "Error in %s: Failed to set %s!\n", __FUNCTION__, BASE_MAC_WLAN_OFFSET_SYSCFG_KEY);
    }

	LAN_start();
	while(1)
	{
		sleep(30);
	}
    }
#else
    for (i = 0; i < CB_REG_CNT_MAX; i++) {
	obj_l[i] = ( appCallBack* ) malloc ( sizeof ( appCallBack ) );
	obj_l[i]->pGWP_act_EthWanLinkDown = NULL;
	obj_l[i]->pGWP_act_EthWanLinkUP = NULL;
    }

    GWP_act_ProvEntry_callback();
    (void) pthread_join(sysevent_tid, NULL);
    (void) pthread_join(linkstate_tid, NULL);
#endif

    return status;
}
#endif
