
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"

extern struct netif gnetif;

osThreadId_t dhcpPollTaskHandle;
const osThreadAttr_t dhcpPollTask_attributes =
    {
        .name = "dhcpPollTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 256};

osThreadId_t udpServerTaskHandle;
const osThreadAttr_t udpServerTask_attributes =
    {
        .name = "udpServerTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 2096};

void Print_Char(char c)
{
  HAL_UART_Transmit(&huart6, (uint8_t *)&c, 1, 100);
}

void Print_String(char *str)
{
  uint8_t len = strlen(str);
  HAL_UART_Transmit(&huart6, (uint8_t *)str, len, 2000);
}

void Print_IP(uint32_t ip)
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
        osThreadFlagsSet(udpServerTaskHandle, 0x0001U);
      }
      else
      {
        Print_Char('.');

        dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

        /* DHCP timeout */
        if (dhcp->tries > 4)
        {
          /* Stop DHCP */
          dhcp_stop(&gnetif);
          Print_String("\nCould not acquire IP address. DHCP timeout\n");

          osThreadSuspend(dhcpPollTaskHandle);
        }
      }
    }
  }
}

void udpServerTask(void *argument)
{
  err_t err;
  struct netconn *conn;
  struct netbuf *buf;
  char buffer[4096];

  /* this function is similar freertos task notification */
  /* wait until dhcp poll is complete */
  osThreadFlagsWait(0x0001U, osFlagsWaitAny, osWaitForever);

  /* create new udp netconn socket*/
  conn = netconn_new(NETCONN_UDP);

  err = netconn_bind(conn, IP_ADDR_ANY, 5001);

  if (err == ERR_OK)
  {
    while (1)
    {
      err = netconn_recv(conn, &buf);

      if (err == ERR_OK)
      {
        /*  no need netconn_connect here, since the netbuf contains the address */
        if (netbuf_copy(buf, buffer, sizeof(buffer)) != buf->p->tot_len)
        {
          Print_String("netbuf_copy failed\n");
        }
        else
        {
          buffer[buf->p->tot_len] = '\0';

          err = netconn_send(conn, buf);

          if (err != ERR_OK)
          {
            Print_String("netconn_send failed\n");
          }
          else
          {
            Print_String(buffer);
          }
        }
        netbuf_delete(buf);
      }
    }
  }
  else
  {
    netconn_delete(conn);
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);

  /* creat a new task to handle udp server*/
  udpServerTaskHandle = osThreadNew(udpServerTask, NULL, &udpServerTask_attributes);
}
