
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwip/sockets.h"

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
        .stack_size = 1024};

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
  int sock = -1, client;
  struct sockaddr_in server_addr, client_addr;
  socklen_t sin_size;
  int recv_data_len;
  char recv_data[100];

  /* this function is similar freertos task notification */
  /* wait until dhcp poll is complete */
  osThreadFlagsWait(0x0001U, osFlagsWaitAny, osWaitForever);

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock < 0)
  {
    Print_String("Socket error\n");
    osThreadSuspend(tcpServerTaskHandle);
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(7);
  memset(&(server_addr.sin_zero), 0, sizeof(server_addr.sin_zero));

  if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
  {
    Print_String("socket bind failed\n");
    osThreadSuspend(tcpServerTaskHandle);
  }

  if (listen(sock, 5) == -1)
  {
    Print_String("listen error\n");
    osThreadSuspend(tcpServerTaskHandle);
  }

  while (1)
  {
    sin_size = sizeof(struct sockaddr_in);

    client = accept(sock, (struct sockaddr *)&client_addr, &sin_size);

    if (client > 0)
    {
      Print_String("new client connected ip = ");
      Print_String(inet_ntoa(client_addr.sin_addr));
      Print_String("\n");

      /* ideally should have created new thread for every connection */
      do
      {
        recv_data_len = recv(client, recv_data, 100, 0);
        write(client, recv_data, recv_data_len);
      } while (recv_data_len > 0);

      Print_String("disconnected from client side\n");
      closesocket(client);
    }
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);

  /* creat a new task to handle tcp client*/
  tcpServerTaskHandle = osThreadNew(tcpServerTask, NULL, &tcpServerTask_attributes);
}
