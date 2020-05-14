
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwip/tcp.h"

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

static err_t TCP_Server_Receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  if (p != NULL)
  {
    /* acknowledge received packet */
    tcp_recved(tpcb, p->tot_len);

    /* echo */
    tcp_write(tpcb, p->payload, p->tot_len, 1);

    memset(p->payload, 0, p->tot_len);

    pbuf_free(p);
  }
  else if (err == ERR_OK)
  {
    Print_String("client disconnected\n");
    return tcp_close(tpcb);
  }
  return ERR_OK;
}

err_t TCP_Server_Accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
  tcp_recv(newpcb, TCP_Server_Receive);
  Print_String("client connected\n");
  return ERR_OK;
}

/* create tcp server and link srever ip and port*/
void TCP_Server_Create(void)
{
  struct tcp_pcb *spcb = NULL;

  spcb = tcp_new();

  tcp_bind(spcb, IP_ADDR_ANY, 7);

  spcb = tcp_listen(spcb);

  tcp_accept(spcb, TCP_Server_Accept);
}

void User_App_Loop()
{

  uint8_t got_ip_flag = 0;

  struct dhcp *dhcp;

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

        TCP_Server_Create();
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
