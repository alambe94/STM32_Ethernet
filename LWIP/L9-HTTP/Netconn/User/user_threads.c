
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

osThreadId_t tcpServerTaskHandle;
const osThreadAttr_t tcpServerTask_attributes =
    {
        .name = "tcpServerTask",
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

        /* notify tcp thread that we acquired ip */
        osThreadFlagsSet(tcpServerTaskHandle, 0x0001U);
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

void tcpServerTask(void *argument)
{
  struct netconn *conn, *newconn;
  err_t err;
  struct netbuf *buf;
  void *data;
  u16_t len;

  /* this function is similar freertos task notification */
  /* wait until dhcp poll is complete */
  osThreadFlagsWait(0x0001U, osFlagsWaitAny, osWaitForever);

  conn = netconn_new(NETCONN_TCP);

  netconn_bind(conn, IP_ADDR_ANY, 5001);

  netconn_listen(conn);

  while (1)
  {
    err = netconn_accept(conn, &newconn);
    Print_String("connected to client\n");

    if (err == ERR_OK)
    {
      while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
      {
        do
        {
          netbuf_data(buf, &data, &len);
          err = netconn_write(newconn, data, len, NETCONN_COPY);
        } while (netbuf_next(buf) >= 0);

        netbuf_delete(buf);
      }

      Print_String("disconnected from client\n");

      netconn_close(newconn);
      netconn_delete(newconn);
    }
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);

  /* creat a new task to handle tcp server*/
  tcpServerTaskHandle = osThreadNew(tcpServerTask, NULL, &tcpServerTask_attributes);
}
