
#include "main.h"
#include "lwip.h"
#include "usart.h"
#include "gpio.h"
#include "string.h"

#include "lwip/api.h"
#include "tftp_server.h"

#include "fatfs.h"

extern struct netif gnetif;

void *open(const char *fname, const char *mode, u8_t write)
{
  if (write)
  {
    f_open(&USERFile, fname, FA_CREATE_ALWAYS | FA_WRITE);
  }
  else
  {
    f_open(&USERFile, fname, FA_READ);
  }
  return &USERFile;
}

void close(void *handle)
{
  f_close(&USERFile);
}

int read(void *handle, void *buf, int bytes)
{
  int cnt;
  f_read(&USERFile, buf, bytes, &cnt);
  return cnt;
}

int write(void *handle, struct pbuf *p)
{
  int cnt;
  f_write(&USERFile, p->payload, p->len, &cnt);
  return cnt;
}

struct tftp_context ctx =
    {
        .open = open,
        .close = close,
        .read = read,
        .write = write};

/* redirect printf to uart */
int __io_putchar(int ch)
{
  huart6.Instance->DR = (ch);
  while (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_TC) == 0)
    ;
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

uint8_t buffer[_MAX_SS]; /* a work buffer for the f_mkfs() */

void W25Q_FAT_Test()
{
  FRESULT res;
  uint8_t tx[] = "TFTP SERVER test Adadaddaggdshajsdnalfjdkjfwnjk";
  uint8_t rx[100] = "0";

  UINT cnt;

  //W25QXX_Erase_Chip();

  res = f_mount(&USERFatFS, (TCHAR const *)USERPath, 0);

  /*only first time*/
  //res = f_mkfs((TCHAR const*) USERPath, FM_ANY, 0, buffer, sizeof(buffer));
  res = f_open(&USERFile, "STM32F0.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F1.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F2.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F3.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F4.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F7.TXT", FA_CREATE_ALWAYS | FA_WRITE);
  res = f_write(&USERFile, tx, sizeof(tx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F0.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F1.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F2.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F3.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F4.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);

  res = f_open(&USERFile, "STM32F7.TXT", FA_READ);
  res = f_read(&USERFile, rx, sizeof(rx), &cnt);
  res = f_close(&USERFile);
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

  //W25Q_FAT_Test();
  f_mount(&USERFatFS, (TCHAR const *)USERPath, 0);

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

        /* Initialize the TFTP server */
        tftp_init(&ctx);
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
