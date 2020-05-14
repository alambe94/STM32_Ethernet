
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"

/* IP address of udp destination server */
#define SERV_IP_ADDR0 192
#define SERV_IP_ADDR1 168
#define SERV_IP_ADDR2 31
#define SERV_IP_ADDR3 240

/* Port number of udp destination server */
#define SERV_PORT 7

/* lwip ip structure*/
ip_addr_t Server_IP;

/* raw udp socket*/
struct udp_pcb *upcb;

char Message_Buffer[100];
volatile uint32_t Message_Sent_Count = 0;

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
  /*increment message count */
  Message_Sent_Count++;

  /* Free receive pbuf */
  pbuf_free(p);
}

/* create udp client and link srever ip and port*/
void UDP_Client_Create(void)
{
  err_t err;

  /* Create a new UDP control block  */
  upcb = udp_new();

  if (upcb != NULL)
  {
    /* fill lwip ip structure */
    IP4_ADDR(&Server_IP, SERV_IP_ADDR0, SERV_IP_ADDR1, SERV_IP_ADDR2, SERV_IP_ADDR3);

    /* connect to server */
    err = udp_connect(upcb, &Server_IP, SERV_PORT);

    if (err == ERR_OK)
    {
      /* Set a receive callback for the upcb */
      udp_recv(upcb, UDP_Receive_Callback, NULL);
    }
  }
}

void UDP_Client_Send(void)
{
  struct pbuf *p;

  sprintf(Message_Buffer, "sending udp client message %li\n", Message_Sent_Count);

  /* allocate pbuf from pool*/
  p = pbuf_alloc(PBUF_TRANSPORT, strlen(Message_Buffer), PBUF_POOL);

  if (p != NULL)
  {
    /* copy data to pbuf */
    pbuf_take(p, Message_Buffer, strlen(Message_Buffer));

    /* send udp data */
    udp_send(upcb, p);

    /* free pbuf */
    pbuf_free(p);
  }
}

void User_App_Loop()
{

  uint8_t got_ip_flag = 0;

  struct dhcp *dhcp;

  UDP_Client_Create();

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

    /* if ip is acquired sent message to server*/
    if (got_ip_flag)
    {
      static uint32_t time_stamp = 0;

      /* spam every 100ms */
      if (HAL_GetTick() - time_stamp > 1000)
      {
        UDP_Client_Send();
        time_stamp = HAL_GetTick();
      }
    }
  }
}
