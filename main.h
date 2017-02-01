#ifndef __main_h_
#define __main_h_
#include <mqx.h>
#include <bsp.h>



#include <shell.h>
#define APPLICATION_HAS_SHELL

#include <rtcs.h>
#ifdef APPLICATION_HAS_SHELL
#include <sh_rtcs.h>
#endif
#include <ipcfg.h>



#define MAIN_TASK 	1
#define BUTTON_TASK	2
#define FLASH_TASK	3

#define ENET_DEVICE 0
#define RTCS_DHCP 0

#define ENET_IPADDR IPADDR(192,168,105,212)
#define ENET_IPMASK	IPADDR(255,255,255,0)
#define ENET_GATEWAY IPADDR(192,168,105,250)
#define RTCS_PPP 0

    

extern void Main_task (uint_32);
extern void Flash_task (uint_32);
extern void Button_task (uint_32);


void rtcs_init(void);






/* PPP device must be set manually and 
** must be different from the default IO channel (BSP_DEFAULT_IO_CHANNEL) 
*/
#define PPP_DEVICE      "ittyb:" 

/*
** Define PPP_DEVICE_DUN only when using PPP to communicate
** to Win9x Dial-Up Networking over a null-modem
** This is ignored if PPP_DEVICE is not #define'd
*/
#define PPP_DEVICE_DUN  1

/*
** Define the local and remote IP addresses for the PPP link
** These are ignored if PPP_DEVICE is not #define'd
*/
#define PPP_LOCADDR     IPADDR(192,168,0,216)
#define PPP_PEERADDR    IPADDR(192,168,0,217)

/*
** Define a default gateway
*/
#define GATE_ADDR       IPADDR(192,168,0,1)


#endif /* __main_h_ */

