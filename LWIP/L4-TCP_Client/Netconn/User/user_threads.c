
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"

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

osThreadId_t dhcpPollTaskHandle;
const osThreadAttr_t dhcpPollTask_attributes =
    {
        .name = "dhcpPollTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 256};

osThreadId_t tcpClientTaskHandle;
const osThreadAttr_t tcpClientTask_attributes =
    {
        .name = "tcpClientTask",
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
        osThreadFlagsSet(tcpClientTaskHandle, 0x0001U);
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

void tcpClientTask(void *argument)
{
  /* lwip ip structure*/
  ip_addr_t Server_IP;

  struct netbuf *rx_net_buf;
  void *rx_data;
  u16_t rx_len;

  /* this function is similar freertos task notification */
  /* wait until dhcp poll is complete */
  osThreadFlagsWait(0x0001U, osFlagsWaitAny, osWaitForever);

  /* fill lwip ip structure */
  IP4_ADDR(&Server_IP, SERV_IP_ADDR0, SERV_IP_ADDR1, SERV_IP_ADDR2, SERV_IP_ADDR3);

  while (1)
  {
    /* create new tcp netconn socket*/
    struct netconn *conn = netconn_new(NETCONN_TCP);

    /* connect to server */
    netconn_connect(conn, &Server_IP, SERV_PORT);

    /* format message to be sent */
    sprintf(Message_Buffer, "sending tcp client message %lu\n", Message_Sent_Count);

    /* send network packet */
    netconn_write(conn, Message_Buffer, strlen(Message_Buffer), NETCONN_COPY);

    /* receive response */
    netconn_recv(conn, &rx_net_buf);

    netbuf_data(rx_net_buf, &rx_data, &rx_len);
    Print_String("response from server -> ");
    Print_String((char *)rx_data);
    Print_String("\n");

    Message_Sent_Count++;

    /* clean up */
    netbuf_delete(rx_net_buf);
    netconn_close(conn);
    netconn_delete(conn);

    osDelay(100);
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);

  /* creat a new task to handle tcp client*/
  tcpClientTaskHandle = osThreadNew(tcpClientTask, NULL, &tcpClientTask_attributes);
}
