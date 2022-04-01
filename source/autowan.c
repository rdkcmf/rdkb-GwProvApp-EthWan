/*
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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

#ifdef AUTOWAN_ENABLE
#include <stdio.h>
#include <string.h>
#include<unistd.h> 
#include <stdlib.h>
#include<errno.h> 
#include<sys/types.h> 
#include<sys/stat.h> 
#include<fcntl.h> 
#include "autowan.h"
#include "ansc_wrapper_base.h"
#include "cm_hal.h"
#include "gw_prov_ethwan.h"
#include "ccsp_hal_ethsw.h"
#include <sysevent/sysevent.h>
#include <syscfg/syscfg.h> 
#ifdef FEATURE_SUPPORT_RDKLOG
#include "rdk_debug.h"
#endif
#define TRUE 1
#define FALSE 0
#define BOOL char
//#define _SIMULATE_PC_

#define AUTO_WAN_LOG GWPROVETHWANLOG
//#define AUTO_WAN_LOG printf
#define WAN_MODE_AUTO		0
#define WAN_MODE_ETH		1
#define WAN_MODE_DOCSIS		2
#define WAN_MODE_UNKNOWN	3

#define AUTOWAN_RETRY_CNT	3
#define AUTOWAN_RETRY_INTERVAL	80 /* TBD */
/*#define MAC_ADDR_LEN    6

typedef struct mac_addr
{
    char hw[ MAC_ADDR_LEN ];
} macaddr_t; */

int g_CurrentWanMode 		= 0;
int g_LastKnowWanMode 		= 0;
int g_SelectedWanMode 		= 0;
int g_AutoWanRetryCnt 		= 0;
int g_AutoWanRetryInterval 	= 0;

#if defined (_BRIDGE_UTILS_BIN_)
#define ONEWIFI_ENABLED "/etc/onewifi_enabled"
#define OPENVSWITCH_LOADED "/sys/module/openvswitch"
    int g_OvsEnable             = 0;
#endif
void SetCurrentWanMode(int mode);
void ManageWanModes(int mode);
void IntializeAutoWanConfig();
int GetCurrentWanMode();
int GetSelectedWanMode();
int GetLastKnownWanMode();
void CheckAltWan();
void CheckWanModeLocked();
void* WanMngrThread(void * arg);
void SelectedWanMode(int mode);
void SetLastKnownWanMode(int mode);
void HandleAutoWanMode(void);
void TryAltWan(int *mode);
int CheckWanStatus();
int CheckWanConnection(int mode);
void RevertTriedConfig(int mode);
void AutoWan_BkupAndReboot();
int
CosaDmlEthWanSetEnable
    (
        BOOL                       bEnable
    );

char* WanModeStr(int WanMode)
{
    if(WanMode == WAN_MODE_AUTO)
    {
         return "WAN_MODE_AUTO";
    }
    if(WanMode == WAN_MODE_ETH)
    {
         return "WAN_MODE_ETH";
    }
    if(WanMode == WAN_MODE_DOCSIS)
    {
         return "WAN_MODE_DOCSIS";
    }
    if(WanMode == WAN_MODE_UNKNOWN)
    {
         return "WAN_MODE_UNKNOWN";
    }
    return "";
}
void LogWanModeInfo()
{
    AUTO_WAN_LOG("CurrentWanMode  - %s\n",WanModeStr(g_CurrentWanMode));
    AUTO_WAN_LOG("SelectedWanMode - %s\n",WanModeStr(g_SelectedWanMode));
    AUTO_WAN_LOG("LastKnowWanMode - %s\n",WanModeStr(g_LastKnowWanMode));
}

void AutoWAN_main()
{
    int thread_status = 0;
    static pthread_t AutoWAN_tid;
    IntializeAutoWanConfig();
    #if defined (_BRIDGE_UTILS_BIN_)
      char buf[ 8 ];
      if( 0 == syscfg_get( NULL, "mesh_ovs_enable", buf, sizeof( buf ) ) )
      {
          if ( strcmp (buf,"true") == 0 )
            g_OvsEnable = 1;
          else 
            g_OvsEnable = 0;

      }
      else
      {
          AUTO_WAN_LOG("syscfg_get failed to retrieve ovs_enable\n");

      }
      if( (0 == access( ONEWIFI_ENABLED, F_OK )) || (0 == access( OPENVSWITCH_LOADED, F_OK )) )
      {
          AUTO_WAN_LOG("g_OvsEnable is set to 1 for OneWifi build\n");
          g_OvsEnable = 1;
      }
    #endif 
    thread_status = pthread_create(&AutoWAN_tid, NULL, WanMngrThread, NULL);
        if (thread_status == 0)
        {
            AUTO_WAN_LOG("WanMngrThread thread created successfully\n");
        }
        else
        {
            AUTO_WAN_LOG("%s error occured while creating WanMngrThread thread\n", strerror(errno));
            
        }
}


void IntializeAutoWanConfig()
{
    AUTO_WAN_LOG("%s\n",__FUNCTION__);
    g_CurrentWanMode        = WAN_MODE_UNKNOWN;
    g_LastKnowWanMode       = WAN_MODE_ETH;//WAN_MODE_UNKNOWN;
    g_SelectedWanMode       = WAN_MODE_AUTO;
    g_AutoWanRetryCnt       = AUTOWAN_RETRY_CNT;
    g_AutoWanRetryInterval  = AUTOWAN_RETRY_INTERVAL;
    
    char out_value[20];

    if (!syscfg_get(NULL, "selected_wan_mode", out_value, sizeof(out_value)))
    {
       g_SelectedWanMode = atoi(out_value);
       AUTO_WAN_LOG("AUTOWAN %s Selected WAN mode = %s\n",__FUNCTION__,WanModeStr(g_SelectedWanMode));
    }
    else
    {
       SelectedWanMode(WAN_MODE_ETH);
       AUTO_WAN_LOG("AUTOWAN %s AUTOWAN is not enabled, Selected WAN mode - %s\n",__FUNCTION__,WanModeStr(g_SelectedWanMode));
    }
    SetCurrentWanMode(WAN_MODE_UNKNOWN);
    LogWanModeInfo();

}

int GetCurrentWanMode()
{
    return g_CurrentWanMode;
}

void SetCurrentWanMode(int mode)
{
    g_CurrentWanMode = mode;
    AUTO_WAN_LOG("%s Set Current WanMode = %s\n",__FUNCTION__, WanModeStr(g_CurrentWanMode)); 
    if (syscfg_set_u_commit(NULL, "curr_wan_mode", (unsigned long) mode) != 0)
    {
    	AUTO_WAN_LOG("syscfg_set failed for curr_wan_mode\n");
    }
}

int GetSelectedWanMode()
{
    return g_SelectedWanMode;
}

void SelectedWanMode(int mode)
{
    g_SelectedWanMode = mode;
    AUTO_WAN_LOG("%s Set  SelectedWanMode = %s\n",__FUNCTION__, WanModeStr(g_SelectedWanMode)); 
    if (syscfg_set_u_commit(NULL, "selected_wan_mode", (unsigned long) mode) != 0)
    {
        AUTO_WAN_LOG("syscfg_set failed for curr_wan_mode\n");
    }
}

int GetLastKnownWanMode()
{
    return g_LastKnowWanMode;
}

void SetLastKnownWanMode(int mode)
{
    g_LastKnowWanMode = mode;
    AUTO_WAN_LOG("%s Set Last Known WanMode = %s\n",__FUNCTION__, WanModeStr(g_LastKnowWanMode));
    if (syscfg_set_u_commit(NULL, "last_wan_mode", (unsigned long) mode) != 0)
    {
        AUTO_WAN_LOG("syscfg_set failed for last_wan_mode\n");
    }
}

void* WanMngrThread(void* arg)
{
    UNREFERENCED_PARAMETER(arg);
    AUTO_WAN_LOG("%s\n",__FUNCTION__);
    pthread_detach(pthread_self());
    AUTO_WAN_LOG("%s Check if AutoWan is Enabled\n",__FUNCTION__);
    switch (GetSelectedWanMode()) 
    { 
       case WAN_MODE_AUTO:
        AUTO_WAN_LOG("Auto WAN Mode is enabled, try Last known WAN mode\n");
        HandleAutoWanMode();
               break;
 
       case WAN_MODE_ETH:
        AUTO_WAN_LOG("Booting-Up in SelectedWanMode - %s\n",WanModeStr(GetSelectedWanMode()));
        SetLastKnownWanMode(WAN_MODE_ETH);
        SetCurrentWanMode(WAN_MODE_ETH);
#if defined(INTEL_PUMA7)
        system("cmctl down");
#endif
#ifdef _SIMULATE_PC_
        system("killall udhcpc");
        system("udhcpc -i eth1 &");
#endif
        break;
 
       case WAN_MODE_DOCSIS:
        AUTO_WAN_LOG("Booting-Up in SelectedWanMode - %s\n",WanModeStr(GetSelectedWanMode())); 
        SetLastKnownWanMode(WAN_MODE_DOCSIS);
        SetCurrentWanMode(WAN_MODE_DOCSIS);
#ifdef _SIMULATE_PC_
        system("killall udhcpc");
        system("udhcpc -i eth2 &");
 #endif
        break;
 
       default: AUTO_WAN_LOG("This is not expected, setting WAN mode to Auto\n");
        SelectedWanMode(WAN_MODE_AUTO);
        HandleAutoWanMode();
        break;   
    } 
    return NULL;
}

void HandleAutoWanMode(void)
{
    AUTO_WAN_LOG("%s\n",__FUNCTION__);
    if(WAN_MODE_UNKNOWN == GetLastKnownWanMode())
    {
        AUTO_WAN_LOG("Last known WAN mode is Unknown\n");	
    }
    switch (GetLastKnownWanMode()) 
    { 
       case WAN_MODE_ETH:
        AUTO_WAN_LOG("Booting-Up in Last known WanMode - %s\n",WanModeStr(GetLastKnownWanMode()));
#ifdef _SIMULATE_PC_
        system("killall udhcpc");
        system("udhcpc -i eth1 &");
#endif
        ManageWanModes(WAN_MODE_ETH);
        break;
 
       case WAN_MODE_DOCSIS:
        AUTO_WAN_LOG("Booting-Up in Last known WanMode - %s\n",WanModeStr(GetLastKnownWanMode()));
#ifdef _SIMULATE_PC_
        system("killall udhcpc");
        system("udhcpc -i eth2 &");
#endif
        ManageWanModes(WAN_MODE_DOCSIS);
        break;

       case WAN_MODE_UNKNOWN:
        AUTO_WAN_LOG("Booting-Up in Last known WanMode - %s\n",WanModeStr(GetLastKnownWanMode()));
#ifdef _SIMULATE_PC_
        system("killall udhcpc");
        system("udhcpc -i eth2 &");
#endif 
        ManageWanModes(WAN_MODE_DOCSIS); 
        break;
 
       default: AUTO_WAN_LOG("This is not expected, setting WAN mode to Auto\n");
        SelectedWanMode(WAN_MODE_AUTO);
        ManageWanModes(WAN_MODE_DOCSIS); 
        break;   
    } 
}

void ManageWanModes(int mode)
{
    int try_mode = mode;
    int ret = 0;
    while(1)
    {
    ret = CheckWanConnection(try_mode);
        if(ret == 1)
        {
           SetLastKnownWanMode(try_mode);
           SetCurrentWanMode(try_mode);
           system("touch /tmp/autowan_iface_finalized");
           if(try_mode == mode)
           {
              AUTO_WAN_LOG("%s - WanMode %s is Locked, Set Current operational mode, reboot is not required, CheckWanConnection=%d\n",__FUNCTION__,WanModeStr(mode),ret);

#if defined(INTEL_PUMA7)
              if(try_mode == WAN_MODE_ETH)
              {
                 AUTO_WAN_LOG("%s - Shutting down DOCSIS\n", __FUNCTION__);
                 system("cmctl down");
              }
#endif
           } // try_mode == mode
           else
           {
              AUTO_WAN_LOG("%s - WanMode %s is Locked, Set Current operational mode, rebooting... \n",__FUNCTION__,WanModeStr(try_mode));
              AutoWan_BkupAndReboot();
           }
           break;
        } // ret == 1
        else if(ret == 2)
        {
           SetLastKnownWanMode(mode);
           SetCurrentWanMode(mode);
           system("touch /tmp/autowan_iface_finalized");
           AUTO_WAN_LOG("%s - WanMode %s is Locked, Set Current operational mode, reboot is not required, CheckWanConnection=%d\n",__FUNCTION__,WanModeStr(mode),ret);
#if defined(INTEL_PUMA7)
           if(try_mode == WAN_MODE_ETH)
           {
              AUTO_WAN_LOG("%s - Shutting down DOCSIS\n", __FUNCTION__);
              system("cmctl down");
           }
#endif
           RevertTriedConfig(mode);
           break;
        } // ret == 2
        else
        {
           TryAltWan(&try_mode);
        }
    } // while(1)
}

int CheckWanConnection(int mode)
{
    int retry = 0;
    int WanLocked = 0;
    int ret = 0;

    while(retry < AUTOWAN_RETRY_CNT)
    {
        retry++;
        sleep(AUTOWAN_RETRY_INTERVAL);
        ret = CheckWanStatus(mode);
        if(ret == 1)
        {
            AUTO_WAN_LOG("%s - Trying %s retry count %d\n",__FUNCTION__,WanModeStr(mode),retry);
        }
        else if(ret == 2)
        {
        AUTO_WAN_LOG("%s - WanMode %s is Locked %d\n",__FUNCTION__,WanModeStr(GetLastKnownWanMode()),retry);
        WanLocked = 2;
        break;
        } 
        else
        {
            AUTO_WAN_LOG("%s - WanMode %s is Locked\n",__FUNCTION__,WanModeStr(mode));
        WanLocked = 1;
        break;
        }
    }
    return WanLocked;
}

extern int sysevent_fd_gs;
extern token_t sysevent_token_gs;
// For Gw_prov_ethwan : Ethwan running mode
int CheckWanStatus(int mode)
{
   char buff[256] = {0};
   char command[256];
   char wan_connection_ifname[ETHWAN_INTERFACE_NAME_MAX_LENGTH] = {0};
   FILE *fp;
   char out_value[20];

    sysevent_get(sysevent_fd_gs, sysevent_token_gs, "current_wan_state", buff, sizeof(buff));

    if (!strcmp(buff, "up")) {
        if(mode == WAN_MODE_ETH)
        {
           syscfg_get(NULL, "wan_physical_ifname", out_value, sizeof(out_value));

           AUTO_WAN_LOG("%s - syscfg returned wan_physical_ifname= %s\n",__FUNCTION__,out_value);

           if(0 != strnlen(out_value,sizeof(out_value)))
           {
              snprintf(wan_connection_ifname, sizeof(wan_connection_ifname), "%s", out_value);
           }
           else
           {
              snprintf(wan_connection_ifname, sizeof(wan_connection_ifname), "%s", WAN_PHY_NAME);
           }

           AUTO_WAN_LOG("%s - wan_connection_ifname= %s\n",__FUNCTION__,wan_connection_ifname);

           /* Validate IPv4 Connection on ETHWAN interface */
           snprintf(command, sizeof(command), "ifconfig %s |grep -i 'inet ' |awk '{print $2}' |cut -f2 -d:", wan_connection_ifname);
           memset(buff,0,sizeof(buff));

           /* Open the command for reading. */
           fp = popen(command, "r");
           if (fp == NULL)
           {
              printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);

           }
           else
           {
               /* Read the output a line at a time - output it. */
               if (fgets(buff, 50, fp) != NULL)
               {
                  printf("IP :%s", buff);
               }
               /* close */
               pclose(fp);
               if(buff[0] != 0)
               {
                  return 2; // Shirish To-Do // Call validate IP function for GLOBAL IP check
               }
           } // fp == NULL

           /* Validate IPv6 Connection on ETHWAN interface */
           snprintf(command,sizeof(command), "ifconfig %s |grep -i 'inet6 ' |grep -i 'Global' |awk '{print $3}'", wan_connection_ifname);
           memset(buff,0,sizeof(buff));

           /* Open the command for reading. */
           fp = popen(command, "r");
           if (fp == NULL)
           {
              printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
           }
           else
           {
              /* Read the output a line at a time - output it. */
              if (fgets(buff, 50, fp) != NULL)
              {
                 printf("IP :%s", buff);
              }
              /* close */
              pclose(fp);
              if(buff[0] != 0)
              {
                 return 2;
              }
           } // fp == NULL)

           return 1;
        } /* (mode == WAN_MODE_ETH)*/
    } /* (!strcmp(buff, "up"))*/

    if(mode == WAN_MODE_DOCSIS)
    {
        /* TODO - RDKB Needs to make this a generic design for devices with DOCSIS interface on Host Processor and devices where it's on the PEER Processor */
        /* DOCSIS on PEER Processor */
#if defined(INTEL_PUMA7)
        char *found = NULL;
        int ret = 0;

        /* Get DOCSIS Connection CMStatus */
        ret = docsis_getCMStatus(buff);
        if( ret == RETURN_ERR )
        {
            AUTO_WAN_LOG("AUTOWAN failed to get CMStatus\n");
            return 1;
        }
        /* Validate DOCSIS Connection CMStatus */
        AUTO_WAN_LOG("%s - CM Status = %s\n", __FUNCTION__, buff);
        found = strstr(buff, "OPERATIONAL");
        if(found)
        {
            AUTO_WAN_LOG("AUTOWAN DOCSIS wan locked\n");
            return 0;
        }
        else
        {
            AUTO_WAN_LOG("AUTOWAN DOCSIS wan not locked\n");
            return 1;
        }
#else
        /* DOCSIS on HOST Processor */

        /* Validate IPv4 Connection on DOCSIS interface */
        snprintf(command,sizeof(command), "ifconfig %s |grep -i 'inet ' |awk '{print $2}' |cut -f2 -d:", DOCSIS_INF_NAME);
        memset(buff,0,sizeof(buff));

        /* Open the command for reading. */
        fp = popen(command, "r");
        if (fp == NULL)
        {
           printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
        }
        else
        {
           /* Read the output a line at a time - output it. */
           if (fgets(buff, 50, fp) != NULL)
           {
              printf("IP :%s", buff);
           }
           /* close */
           pclose(fp);
           if(buff[0] != 0)
           {
              return 0; // Shirish To-Do // Call validate IP function for GLOBAL IP check
           }
        } // fp == NULL

        /* Validate IPv6 Connection on DOCSIS interface */
        snprintf(command,sizeof(command), "ifconfig %s |grep -i 'inet6 ' |grep -i 'Global' |awk '{print $3}'", DOCSIS_INF_NAME);
        memset(buff,0,sizeof(buff));

        /* Open the command for reading. */
        fp = popen(command, "r");
        if (fp == NULL)
        {
           printf("<%s>:<%d> Error popen\n", __FUNCTION__, __LINE__);
        }
        else
        {
           /* Read the output a line at a time - output it. */
           if (fgets(buff, 256, fp) != NULL)
           {
             printf("IP :%s", buff);
           }
           /* close */
           pclose(fp);
           if(buff[0] != 0)
           {
              return 0;
           }
        } // fp == NULL
#endif
    } // mode == WAN_MODE_DOCSIS

return 1;
}

void TryAltWan(int *mode)
{
    char pRfSignalStatus = 0;
    char ethwan_ifname[ETHWAN_INTERFACE_NAME_MAX_LENGTH] = {0};
    char command[64+5] = {0};

#if !defined(AUTO_WAN_ALWAYS_RECONFIG_EROUTER)
    char wanPhyName[20] = {0};
    char out_value[20];

     syscfg_get(NULL, "wan_physical_ifname", out_value, sizeof(out_value));

     AUTO_WAN_LOG("%s - syscfg returned wan_physical_ifname= %s\n",__FUNCTION__,out_value);

     if(0 != strnlen(out_value,sizeof(out_value)))
     {
        snprintf(wanPhyName, sizeof(wanPhyName), "%s", out_value);
     }
     else
     {
        snprintf(wanPhyName, sizeof(wanPhyName), "%s", WAN_PHY_NAME);
     }
#endif

     if ( (0 != GWP_GetEthWanInterfaceName(ethwan_ifname, sizeof(ethwan_ifname)))
          || (0 == strnlen(ethwan_ifname,sizeof(ethwan_ifname)))
          || (0 == strncmp(ethwan_ifname,"disable",sizeof(ethwan_ifname)))
        )
     {
         /* Fallback case needs to set it default */
         snprintf(ethwan_ifname ,sizeof(ethwan_ifname), "%s", ETHWAN_INF_NAME);
     }

     /* During WAN Disruptions XBs were not coming back online without another reboot. Toggle interface between modes */
     AUTO_WAN_LOG("%s - Toggling ethwan_ifname= %s DOWN\n",__FUNCTION__,ethwan_ifname);

     snprintf(command,sizeof(command),"ip link set dev %s down",ethwan_ifname);
     system(command);

     sleep(5);
     AUTO_WAN_LOG("%s - Toggling ethwan_ifname= %s UP\n",__FUNCTION__,ethwan_ifname);

     snprintf(command,sizeof(command),"ip link set dev %s up",ethwan_ifname);
     system(command);

    if(*mode == WAN_MODE_DOCSIS)
    {
        *mode = WAN_MODE_ETH;

        CosaDmlEthWanSetEnable(TRUE);

#if !defined(AUTO_WAN_ALWAYS_RECONFIG_EROUTER)

        AUTO_WAN_LOG("%s - mode= %s ethwan_ifname= %s, wanPhyName= %s\n",__FUNCTION__,WanModeStr(WAN_MODE_ETH),ethwan_ifname,wanPhyName);
  
        #if defined (_BRIDGE_UTILS_BIN_)

            if ( syscfg_set_commit( NULL, "eth_wan_iface_name", ethwan_ifname ) != 0 )
            {
                AUTO_WAN_LOG( "syscfg_set failed for eth_wan_iface_name\n" );
            }

          if (g_OvsEnable)
          {
              snprintf(command,sizeof(command),"/usr/bin/bridgeUtils del-port brlan0 %s",ethwan_ifname);
          }
          else
          {
              snprintf(command,sizeof(command),"brctl delif brlan0 %s",ethwan_ifname);
          }
        #else
            snprintf(command,sizeof(command),"brctl delif brlan0 %s",ethwan_ifname);
        #endif

        system(command);
        //  system("brctl delif brlan0 eth3");
#if defined(_COSA_BCM_ARM_)
        snprintf(command,sizeof(command),"brctl addif %s %s",wanPhyName, DOCSIS_INF_NAME);
        system(command);
        //  system("brctl addif erouter0 cm0");
#endif
        snprintf(command,sizeof(command),"udhcpc -i %s &",ethwan_ifname);
        system(command);    
        //  system("rm -rf /tmp/wan_locked; udhcpc -i eth3 &");
        snprintf(command,sizeof(command),"sysctl -w net.ipv6.conf.%s.accept_ra=2",ethwan_ifname);
        system(command);
        //  system("sysctl -w net.ipv6.conf.eth3.accept_ra=2");
        snprintf(command,sizeof(command),"udhcpc -i %s &",ethwan_ifname);
        system(command);
        //  system("udhcpc -i eth3 &");
        system("killall udhcpc");
#endif
    } // *mode == WAN_MODE_DOCSIS
    else
    {
#if defined(INTEL_PUMA7)
       AUTO_WAN_LOG("%s - Bringing Up DOCSIS\n", __FUNCTION__);
       system("cmctl up");
#endif

       if(RETURN_ERR == docsis_IsEnergyDetected(&pRfSignalStatus)) {
          AUTO_WAN_LOG("AUTOWAN Failed to get RfSignalStatus \n");
       }

       if(pRfSignalStatus == 0)
       {
          AUTO_WAN_LOG("%s - Trying Alternet WanMode - %s\n",__FUNCTION__,WanModeStr(WAN_MODE_DOCSIS));
          AUTO_WAN_LOG("%s - Alternet WanMode - %s not present\n",__FUNCTION__,WanModeStr(WAN_MODE_DOCSIS));
          return ;
       }

       AUTO_WAN_LOG("AUTOWAN- %s Dosis present  - %d\n",__FUNCTION__,pRfSignalStatus);
       *mode = WAN_MODE_DOCSIS;
       CosaDmlEthWanSetEnable(FALSE);

#if !defined(AUTO_WAN_ALWAYS_RECONFIG_EROUTER)
        system("killall udhcpc");
#if defined(_COSA_BCM_ARM_)
        AUTO_WAN_LOG("%s - mode= %s wanPhyName= %s\n",__FUNCTION__,WanModeStr(WAN_MODE_DOCSIS),wanPhyName);

        snprintf(command,sizeof(command),"brctl delif %s %s",wanPhyName, DOCSIS_INF_NAME);
        system(command);
        //system("brctl delif erouter0 cm0");
        snprintf(command,sizeof(command),"udhcpc -i %s &",DOCSIS_INF_NAME);
        system(command);    
        //system("/tmp/wan_locked; udhcpc -i cm0 &");
        snprintf(command,sizeof(command),"sysctl -w net.ipv6.conf.%s.accept_ra=2",DOCSIS_INF_NAME);
        system(command);
        //system("sysctl -w net.ipv6.conf.cm0.accept_ra=2");
        //  system("udhcpc -i cm0 &");
#endif
#endif
    }
    AUTO_WAN_LOG("%s - Trying Alternet WanMode - %s\n",__FUNCTION__,WanModeStr(*mode));
}

void RevertTriedConfig(int mode)
{
    char command[64+5];
    char ethwan_ifname[ETHWAN_INTERFACE_NAME_MAX_LENGTH] = {0};

    if(mode == WAN_MODE_DOCSIS)
    {
        if ( (0 != GWP_GetEthWanInterfaceName(ethwan_ifname, sizeof(ethwan_ifname)))
             || (0 == strnlen(ethwan_ifname,sizeof(ethwan_ifname)))
             || (0 == strncmp(ethwan_ifname,"disable",sizeof(ethwan_ifname)))
           )
        {
           /* Fallback case needs to set it default */
           snprintf(ethwan_ifname ,sizeof(ethwan_ifname), "%s", ETHWAN_INF_NAME);
        }

        AUTO_WAN_LOG("%s - ethwan_ifname= %s\n",__FUNCTION__,ethwan_ifname);
        #if defined (_BRIDGE_UTILS_BIN_)

            if ( syscfg_set_commit( NULL, "eth_wan_iface_name", ethwan_ifname ) != 0 )
            {
                AUTO_WAN_LOG( "syscfg_set failed for eth_wan_iface_name\n" );
            }
        #endif
        snprintf(command,sizeof(command),"ifconfig %s down",ethwan_ifname);
        system(command);
        //system("ifconfig eth3 down");
        snprintf(command,sizeof(command),"ip addr flush dev %s",ethwan_ifname);
        system(command);
        //system("ip addr flush dev eth3");
        snprintf(command,sizeof(command),"ip -6 addr flush dev %s",ethwan_ifname);
        system(command);
        //system("ip -6 addr flush dev eth3");
        snprintf(command,sizeof(command),"sysctl -w net.ipv6.conf.%s.accept_ra=0",ethwan_ifname);
        system(command);
        //system("sysctl -w net.ipv6.conf.eth3.accept_ra=0");
        snprintf(command,sizeof(command),"ifconfig %s up",ethwan_ifname);
        system(command);
        //system("ifconfig eth3 up");
        #if defined (_BRIDGE_UTILS_BIN_)

          if (g_OvsEnable)
          {
              snprintf(command,sizeof(command),"/usr/bin/bridgeUtils add-port brlan0 %s",ethwan_ifname);
          }
          else
          {
              snprintf(command,sizeof(command),"brctl addif brlan0 %s",ethwan_ifname);
          }
        #else
          snprintf(command,sizeof(command),"brctl addif brlan0 %s",ethwan_ifname);
        #endif

        system(command);
        //system("brctl addif brlan0 eth3");
#if defined(_COSA_BCM_ARM_)
        snprintf(command,sizeof(command),"brctl addif erouter0 %s",DOCSIS_INF_NAME);
        system(command);
        //system("brctl addif erouter0 cm0");
#endif 
    }
    else
    {
#if defined(_COSA_BCM_ARM_)
        snprintf(command,sizeof(command),"ip addr flush dev %s",DOCSIS_INF_NAME);
        system(command);
        //system("ip addr flush dev cm0");
        snprintf(command,sizeof(command),"ip -6 addr flush dev %s",DOCSIS_INF_NAME);
        system(command);
        //system("ip -6 addr flush dev cm0");
        snprintf(command,sizeof(command),"sysctl -w net.ipv6.conf.%s.accept_ra=0",DOCSIS_INF_NAME);
        system(command);
        //system("sysctl -w net.ipv6.conf.cm0.accept_ra=0");
        snprintf(command,sizeof(command),"brctl addif erouter0 %s",DOCSIS_INF_NAME);
        system(command);
        //system("brctl addif erouter0 cm0");
#endif
    }
}
int
CosaDmlEthWanSetEnable
    (
        BOOL                       bEnable
    )
{
#if ((defined (_COSA_BCM_ARM_) && !defined(_CBR_PRODUCT_REQ_) && !defined(_PLATFORM_RASPBERRYPI_) && !defined(_PLATFORM_TURRIS_)) || defined(INTEL_PUMA7) || defined(_CBR2_PRODUCT_REQ_))
        //BOOL bGetStatus = FALSE;
        //CcspHalExtSw_getEthWanEnable(&bGetStatus);
    //if (bEnable != bGetStatus)
#if !defined(AUTO_WAN_ALWAYS_RECONFIG_EROUTER)
    {
       char command[64];
       if(bEnable == FALSE)
       {
        system("ifconfig erouter0 down");
        snprintf(command,sizeof(command),"ip link set erouter0 name %s",ETHWAN_INF_NAME);
        system(command);
        system("ip link set dummy-rf name erouter0");
        system("ifconfig eth0 up;ifconfig erouter0 up");
        
       } 
    }
#endif

    CcspHalExtSw_setEthWanPort ( ETHWAN_DEF_INTF_NUM );


    if ( RETURN_OK == CcspHalExtSw_setEthWanEnable( bEnable ) ) 
    {
        if(bEnable)
        {
            system("touch /nvram/ETHWAN_ENABLE");
        }
        else
        {
            system("rm /nvram/ETHWAN_ENABLE");
        }

        if ( syscfg_set_commit( NULL, "eth_wan_enabled", bEnable ? "true" : "false" ) != 0 )
        {
            AUTO_WAN_LOG( "syscfg_set failed for eth_wan_enabled\n" );
            return RETURN_ERR;
        }
    }
    return RETURN_OK;
#else
    return RETURN_ERR;
#endif /* (defined (_COSA_BCM_ARM_) && !defined(_CBR_PRODUCT_REQ_)) */
}

void AutoWan_BkupAndReboot()
{
    //OnboardLog("Device reboot due to reason WAN_Mode_Change\n");

    if (syscfg_set_commit(NULL, "X_RDKCENTRAL-COM_LastRebootReason", "WAN_Mode_Change") != 0)
    {
    	    AUTO_WAN_LOG("RDKB_REBOOT : RebootDevice syscfg_set failed GUI\n");
    }

    if (syscfg_set_commit(NULL, "X_RDKCENTRAL-COM_LastRebootCounter", "1") != 0)
    {
    	    AUTO_WAN_LOG("syscfg_set failed\n");
    }

    /* Need to do reboot the device here */
    system("dmcli eRT setv Device.X_CISCO_COM_DeviceControl.RebootDevice string Device");
}
#endif
