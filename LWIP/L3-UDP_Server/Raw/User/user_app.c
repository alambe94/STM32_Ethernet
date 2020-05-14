
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"

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

void UDP_Receive_Callback(void *arg,
                          struct udp_pcb *upcb,
                          struct pbuf *p,
                          const ip_addr_t *addr,
                          u16_t port)
{
    /* Connect to the remote client */
    udp_connect(upcb, addr, port);

    /* Tell the client that we have accepted it */
    udp_send(upcb, p);

    /* free the UDP connection, so we can accept new clients */
    udp_disconnect(upcb);

    /* Free the p buffer */
    pbuf_free(p);
}

/* create udp server and link srever ip and port*/
void UDP_Server_Create(void)
{
    struct udp_pcb *upcb;
    err_t err;

    /* Create a new UDP control block  */
    upcb = udp_new();

    if (upcb)
    {
      /* Bind the upcb to the UDP_PORT port */
      /* Using IP_ADDR_ANY allow the upcb to be used by any local interface */
       err = udp_bind(upcb, IP_ADDR_ANY, 7);

       if(err == ERR_OK)
       {
         /* Set a receive callback for the upcb */
         udp_recv(upcb, UDP_Receive_Callback, NULL);
       }
       else
       {
         udp_remove(upcb);
       }
    }
}

void User_App_Loop()
{

  uint8_t got_ip_flag = 0;

  struct dhcp *dhcp;

  UDP_Server_Create();

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
