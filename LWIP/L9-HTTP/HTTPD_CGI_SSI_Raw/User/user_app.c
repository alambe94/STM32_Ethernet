
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "http_cgi_ssi.h"

extern struct netif gnetif;

/* redirect printf to uart */
int __io_putchar(int ch)
{
  huart6.Instance->DR = (ch);
  while (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_TC) == 0);
  return ch;
}

void Print_Char(char c)
{
  HAL_UART_Transmit(&huart6, (uint8_t *)&c, 1, 100);
}

void Print_String(char *str)
{
  uint8_t len = strlen(str);
  HAL_UART_Transmit(&huart6, (uint8_t *)str, len, 2000);
}

void Print_Int(int32_t num)
{
  char int_to_str[10] = "";
  itoa(num, int_to_str, 10);
  Print_String(int_to_str);
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

void User_App_Loop()
{

  uint8_t got_ip_flag = 0;

  struct dhcp *dhcp;

  /* io buffer off*/
  /* redirect printf to uart */
  setvbuf(stdout, NULL, _IONBF, 0);

  Print_String("DHCP client started\n");
  Print_String("Acquiring IP address\n");

  while (1)
  {
    MX_LWIP_Process();

    if (got_ip_flag == 0)
    {
      if (dhcp_supplied_address(&gnetif))
      {
        got_ip_flag = 1;
        Print_String("\ngot IP:");
        Print_IP(gnetif.ip_addr.addr);

        http_server_init();
      }
      else
      {
        static uint32_t print_delay = 0;
        print_delay++;
        if (print_delay > 10000)
        {
          Print_Char('.');
          print_delay = 0;
        }

        dhcp = (struct dhcp *)netif_get_client_data(&gnetif, LWIP_NETIF_CLIENT_DATA_INDEX_DHCP);

        /* DHCP timeout */
        if (dhcp->tries > 4)
        {
          /* Stop DHCP */
          dhcp_stop(&gnetif);
          Print_String("\nCould not acquire IP address. DHCP timeout\n");
        }
      }
    }
  }
}
