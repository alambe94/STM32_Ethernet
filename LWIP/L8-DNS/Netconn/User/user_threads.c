
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwip/dns.h"

extern struct netif gnetif;

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


void tcpServerTask(void *argument)
{
  struct netconn *conn, *newconn;
  err_t err;
  struct netbuf *buf;
  void *data;
  u16_t len;

  char data_buff[100];
  ip4_addr_t dns_ip;

  sprintf(data_buff, "enter website name to get ip address\n");

  conn = netconn_new(NETCONN_TCP);

  netconn_bind(conn, IP_ADDR_ANY, 8800);

  netconn_listen(conn);

  while (1)
  {
    err = netconn_accept(conn, &newconn);
    Print_String("connected to client\n");

    if (err == ERR_OK)
    {
      netconn_write(newconn, data_buff, strlen(data_buff), NETCONN_COPY);

      while ((err = netconn_recv(newconn, &buf)) == ERR_OK)
      {
        do
        {
          netbuf_data(buf, &data, &len);
          if(len == 0)
            continue;

          memset(data_buff, 0, 100);
          memcpy(data_buff, data, len);

          err = netconn_gethostbyname(data_buff, &dns_ip);

          if(err == ERR_OK)
          {
            len = sprintf(data_buff,"%s = %s\n", data_buff, ip_ntoa(&dns_ip));
            Print_String(data_buff);
            netconn_write(newconn, data_buff, len, NETCONN_COPY);
          }
          else
          {
            len = sprintf(data_buff,"get host name fail...\n");
            Print_String(data_buff);
            netconn_write(newconn, data_buff, len, NETCONN_COPY);
          }
        } while (netbuf_next(buf) >= 0);

        netbuf_delete(buf);
      }

      Print_String("disconnected from client\n");

      netconn_close(newconn);
      netconn_delete(newconn);
    }
  }
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

        /* creat a new task to handle tcp server*/
        sys_thread_new("tcpServerTask", tcpServerTask, NULL, 1024, osPriorityNormal);
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
          osThreadSuspend(NULL);
        }
      }
    }
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  sys_thread_new("dhcpPollTask", dhcpPollTask, NULL, 256, osPriorityNormal);
}
