
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwip/sockets.h"

/* IP address of tcp destination server */
#define SERV_IP_ADDR0 192
#define SERV_IP_ADDR1 168
#define SERV_IP_ADDR2 31
#define SERV_IP_ADDR3 240

/* Port number of tcp destination server */
#define SERV_PORT 7

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

void tcpClientTask(void *argument)
{
  /* lwip ip structure*/
  ip_addr_t Server_IP;

  int sock = -1;
  struct sockaddr_in addr;

  char rx_data[100];

  /* fill lwip ip structure */
  IP4_ADDR(&Server_IP, SERV_IP_ADDR0, SERV_IP_ADDR1, SERV_IP_ADDR2, SERV_IP_ADDR3);

  /* fill server socket info */
  addr.sin_family = AF_INET;
  addr.sin_port = htons(SERV_PORT);
  addr.sin_addr.s_addr = Server_IP.addr;
  memset(&(addr.sin_zero), 0, sizeof(addr.sin_zero));

  while (1)
  {
    /* create new tcp socket*/
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
      Print_String("Could not create tcp socket\n");
    }

    /* connect to server */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
      Print_String("Connect Failed \n");
    }

    /* format message to be sent */
    sprintf(Message_Buffer, "sending tcp client message %lu\n", Message_Sent_Count);

    /* write to server */
    write(sock, Message_Buffer, sizeof(Message_Buffer));

    /* read response from server */
    read(sock, rx_data, sizeof(Message_Buffer));

    Print_String("response from server -> ");
    Print_String((char *)rx_data);
    Print_String("\n");

    Message_Sent_Count++;

    close(sock);

    osDelay(100);
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

        /* creat a new task to handle tcp client*/
        sys_thread_new("tcpClientTask", tcpClientTask, NULL, 1024, osPriorityNormal);
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
