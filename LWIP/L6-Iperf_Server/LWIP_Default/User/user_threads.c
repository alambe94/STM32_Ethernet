
#include "main.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "lwiperf.h"

extern struct netif gnetif;

osThreadId_t dhcpPollTaskHandle;
const osThreadAttr_t dhcpPollTask_attributes =
    {
        .name = "dhcpPollTask",
        .priority = (osPriority_t)osPriorityNormal,
        .stack_size = 512};

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

void Iperf_Callback(void *arg, enum lwiperf_report_type report_type,
                    const ip_addr_t *local_addr, u16_t local_port,
                    const ip_addr_t *remote_addr, u16_t remote_port,
                    u32_t bytes_transferred, u32_t ms_duration,
                    u32_t bandwidth_kbitpsec)
{
  if (LWIPERF_TCP_DONE_SERVER == report_type)
  {
    Print_String("Iperf test results\n");
    Print_String("Client IP -> ");
    Print_IP(remote_addr->addr);
    Print_String(":");
    Print_Int(remote_port);
    Print_String("\nBytes transfered  = ");
    Print_Int(bytes_transferred);
    Print_String("\nDuration  = ");
    Print_Int(ms_duration);
    Print_String("ms\nSpeed  = ");
    Print_Int(bandwidth_kbitpsec);
    Print_String("kbs/s\n");
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

        /* creat and start iperf server */
        lwiperf_start_tcp_server_default(Iperf_Callback, NULL);
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

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  dhcpPollTaskHandle = osThreadNew(dhcpPollTask, NULL, &dhcpPollTask_attributes);
}
