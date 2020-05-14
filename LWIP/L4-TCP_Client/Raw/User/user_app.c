
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwip/tcp.h"
#include "lwip/memp.h"

/* IP address of udp destination server */
#define SERV_IP_ADDR0 192
#define SERV_IP_ADDR1 168
#define SERV_IP_ADDR2 31
#define SERV_IP_ADDR3 240

/* Port number of udp destination server */
#define SERV_PORT 7

char Message_Buffer[100];
volatile uint32_t Message_Sent_Count = 0;

extern struct netif gnetif;

void TCP_Client_Create(void);

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

err_t TCP_Client_Send(struct tcp_pcb *tpcb)
{
  sprintf(Message_Buffer, "sending tcp client message %li\n", Message_Sent_Count);

  tcp_write(tpcb, Message_Buffer, sizeof(Message_Buffer), 1);

  return ERR_OK;
}

err_t TCP_Client_Sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
  Message_Sent_Count++;
  return ERR_OK;
}

err_t TCP_Client_Receive(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  if (p != NULL)
  {
    /* Acknowledge data reception */
    tcp_recved(tpcb, p->tot_len);

    Print_String("response from server -> ");
    Print_String((char *)p->payload);
    Print_String("\n");

    pbuf_free(p);

    /* close connection */
    tcp_close(tpcb);

    /* just to slow down spamming */
    HAL_Delay(100);

    /* create new tcp client */
    TCP_Client_Create();
  }
  else
  {
    pbuf_free(p);
    tcp_close(tpcb);
  }
  return ERR_OK;
}

void TCP_Client_Error(void *arg, err_t err)
{
  Print_String("connection error\n");
}

err_t TCP_Client_Poll(void *arg, struct tcp_pcb *tpcb)
{
  /* can check for payload transmit complete if payload is fragmented */
  return ERR_OK;
}

err_t TCP_Client_Connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
  if (err == ERR_OK)
  {
    Print_String("connected to server\n");

    /* initialize LwIP tcp_recv callback function */
    tcp_recv(tpcb, TCP_Client_Receive);

    /* initialize LwIP tcp_sent callback function */
    tcp_sent(tpcb, TCP_Client_Sent);

    /* initialize LwIP tcp_poll callback function */
    tcp_poll(tpcb, TCP_Client_Poll, 1);

    /* send data */
    TCP_Client_Send(tpcb);
  }
  else
  {
    tcp_close(tpcb);
  }
  return ERR_OK;
}

/* create tcp client and link srever ip and port*/
void TCP_Client_Create(void)
{
  /* raw tcp socket*/
  struct tcp_pcb *tcp_pcb;

  /* destination IP*/
  ip_addr_t Server_IP;

  /* create new tcp pcb */
  tcp_pcb = tcp_new();

  if (tcp_pcb != NULL)
  {
    /* fill lwip ip structure */
    IP4_ADDR(&Server_IP, SERV_IP_ADDR0, SERV_IP_ADDR1, SERV_IP_ADDR2, SERV_IP_ADDR3);

    /* connect to destination address/port */
    tcp_connect(tcp_pcb, &Server_IP, SERV_PORT, TCP_Client_Connected);

    /* tcp error callback */
    tcp_err(tcp_pcb, TCP_Client_Error);
  }
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

        /* create new tcp client after acquiring ip */
        TCP_Client_Create();
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
