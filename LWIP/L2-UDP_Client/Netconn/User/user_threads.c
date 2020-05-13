
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"

/* IP address of udp destination server */
#define SERV_IP_ADDR0 192
#define SERV_IP_ADDR1 168
#define SERV_IP_ADDR2 31
#define SERV_IP_ADDR3 117

/* Port number of udp destination server */
#define SERV_PORT 7

/* lwip ip structure*/
ip_addr_t Server_IP;

char Message_Buffer[100];
volatile uint32_t Message_Sent_Count = 0;

extern struct netif gnetif;

osThreadId_t dhcpPollTaskHandle;
const osThreadAttr_t dhcpPollTask_attributes =
    {
        .name = "dhcpPollTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 256};

osThreadId_t udpClientTaskHandle;
const osThreadAttr_t udpClientTask_attributes =
    {
        .name = "udpClientTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 256};

void Print_Char(char c)
{
  HAL_UART_Transmit(&huart6, (uint8_t *)&c, 1, 100);
}

void Print_String(char *str)
{
  uint8_t len = strlen(str);
  HAL_UART_Transmit(&huart6, (uint8_t *)str, len, 2000);
}

void Print_IP(unsigned int ip)
{
  char buff[20] = {0};
  uint8_t bytes[4];
  bytes[3] = ip & 0xFF;
  bytes[2] = (ip >> 8) & 0xFF;
  bytes[1] = (ip >> 16) & 0xFF;
  bytes[0] = (ip >> 24) & 0xFF;
  sprintf(buff, "%d.%d.%d.%d\n", bytes[3], bytes[2], bytes[1], bytes[0]);
  HAL_UART_Transmit(&huart6, (uint8_t *)buff, strlen(buff), 1000);
}

void dhcpPollTask(void *argument)
{
  uint8_t got_ip_flag = 0;
  struct dhcp *dhcp;

  Print_String("DHCP client started\n");
  Print_String("Acquiring IP address\n");

  for (;;)
  {
    osDelay(100);

    if (got_ip_flag == 0)
    {
      if (dhcp_supplied_address(&gnetif))
      {
        got_ip_flag = 1;
        Print_String("\ngot IP:");
        Print_IP(gnetif.ip_addr.addr);

        /* notify udp thread that we acquired ip */
        osThreadFlagsSet(udpClientTaskHandle, 0x0001U);
      }
      else
      {
        Print_Char('.');
      }
    }
    else
    {
      dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

      /* DHCP timeout */
      if (dhcp->tries > 4)
      {
        /* Stop DHCP */
        dhcp_stop(&gnetif);
        Print_String("Could not acquire IP address. DHCP timeout\n");

        osThreadSuspend(dhcpPollTaskHandle);
      }
    }
  }
}

void udpClientTask(void *argument)
{
  /* this function is similar freertos task notification */
  /* wait until dhcp poll is complete */
  osThreadFlagsWait(0x0001U, osFlagsWaitAny, osWaitForever);

  /* fill lwip ip structure */
  IP4_ADDR(&Server_IP, SERV_IP_ADDR0, SERV_IP_ADDR1, SERV_IP_ADDR2, SERV_IP_ADDR3);

  /* create new udp netconn socket*/
  struct netconn *conn = netconn_new(NETCONN_UDP);

  /* connect to server */
  netconn_connect(conn, &Server_IP, SERV_PORT);

  /* create new network buffer */
  struct netbuf *net_buf = netbuf_new();

  while (1)
  {
    /* format message to be sent */
    sprintf(Message_Buffer, "sending udp client message %lu", Message_Sent_Count);

    /* link or copy message buffer to network buffer */
    netbuf_ref(net_buf, Message_Buffer, strlen(Message_Buffer));

    /* send network packet */
    netconn_send(conn, net_buf);

    Message_Sent_Count++;

    osDelay(100);
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);

  /* creat a new task to handle udp client*/
  udpClientTaskHandle = osThreadNew(udpClientTask, NULL, &udpClientTask_attributes);
}
