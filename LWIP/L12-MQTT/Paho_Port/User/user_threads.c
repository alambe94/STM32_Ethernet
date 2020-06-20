
#include "main.h"
#include "cmsis_os.h"
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

uint8_t MQTT_Connect(void)
{
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  uint8_t buf[200];
  int buflen = sizeof(buf);
  int len = 0;
  data.clientID.cstring = CLIENT_ID;      //���
  data.keepAliveInterval = KEEPLIVE_TIME; //���ֻ�Ծ
  data.username.cstring = USER_NAME;      //�û���
  data.password.cstring = PASSWORD;       //��Կ
  data.MQTTVersion = MQTT_VERSION;        //3��ʾ3.1�汾��4��ʾ3.11�汾
  data.cleansession = 1;
  //��װ��Ϣ
  len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);
  //������Ϣ
  transport_sendPacketBuffer(buf, len);

  /* �ȴ�������Ӧ */
  if (MQTTPacket_read(buf, buflen, transport_getdata) == CONNACK)
  {
    unsigned char sessionPresent, connack_rc;
    if (MQTTDeserialize_connack(&sessionPresent, &connack_rc, buf, buflen) != 1 || connack_rc != 0)
    {
      PRINT_DEBUG("�޷����ӣ����������: %d��\n", connack_rc);
      return Connect_NOK;
    }
    else
    {
      PRINT_DEBUG("�û�������Կ��֤�ɹ���MQTT���ӳɹ���\n");
      return Connect_OK;
    }
  }
  else
    PRINT_DEBUG("MQTT��������Ӧ��\n");
  return Connect_NOTACK;
}

void Client_Connect(char *broker)
{
  char *broker_ip;
  ip4_addr_t dns_ip;
  int32_t socket_connected = 0;

  MQTTPacket_connectData mqtt_packet = MQTTPacket_connectData_initializer;

  netconn_gethostbyname(broker, &dns_ip);
  broker_ip = ip_ntoa(&dns_ip);
  /** print the ip address of broker */
  printf("%s = %s\n", broker, broker_ip);

  do
  {
    socket_connected = transport_open(broker_ip, 1883);
    vTaskDelay(3000);
  } while (socket_connected < 0);

  mqtt_packet.clientID.cstring = "CLIENT_ID";
  mqtt_packet.keepAliveInterval = 50;
  mqtt_packet.username.cstring = "USER_NAME";
  mqtt_packet.password.cstring = "PASSWORD";
  mqtt_packet.MQTTVersion = 4;
  mqtt_packet.cleansession = 1;
  len = MQTTSerialize_connect((unsigned char *)buf, buflen, &data);

  if (MQTT_Connect() != Connect_OK)
  {
    transport_close();
  }

  if (MQTTSubscribe(MQTT_Socket, (char *)TOPIC, QOS1) < 0)
  {
    transport_close();
  }
}

void mqtt_thread(void *pvParameters)
{
  uint32_t curtick;
  uint8_t no_mqtt_msg_exchange = 1;
  uint8_t buf[MSG_MAX_LEN];
  int32_t buflen = sizeof(buf);
  int32_t type;
  fd_set readfd;
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 10;

MQTT_START:
  //��ʼ����
  Client_Connect();
  //��ȡ��ǰ�δ���Ϊ��������ʼʱ��
  curtick = xTaskGetTickCount();
  while (1)
  {
    //���������ݽ���
    no_mqtt_msg_exchange = 1;

    //������Ϣ
    FD_ZERO(&readfd);
    FD_SET(MQTT_Socket, &readfd);

    //�ȴ��ɶ��¼�
    select(MQTT_Socket + 1, &readfd, NULL, NULL, &tv);

    //�ж�MQTT�������Ƿ�������
    if (FD_ISSET(MQTT_Socket, &readfd) != 0)
    {
      //��ȡ���ݰ�--ע���������Ϊ0��������
      type = ReadPacketTimeout(MQTT_Socket, buf, buflen, 50);
      if (type != -1)
      {
        mqtt_pktype_ctl(type, buf, buflen);
        //���������ݽ���
        no_mqtt_msg_exchange = 0;
        //��ȡ��ǰ�δ���Ϊ��������ʼʱ��
        curtick = xTaskGetTickCount();
      }
    }

    //������ҪĿ���Ƕ�ʱ�����������PING��������
    if ((xTaskGetTickCount() - curtick) > (KEEPLIVE_TIME / 2 * 1000))
    {
      curtick = xTaskGetTickCount();
      //�ж��Ƿ������ݽ���
      if (no_mqtt_msg_exchange == 0)
      {
        //��������ݽ�������ξͲ���Ҫ����PING��Ϣ
        continue;
      }

      if (MQTT_PingReq(MQTT_Socket) < 0)
      {
        //����������
        PRINT_DEBUG("���ͱ��ֻ���pingʧ��....\n");
        goto CLOSE;
      }

      //�����ɹ�
      PRINT_DEBUG("���ͱ��ֻ���ping��Ϊ�����ɹ�....\n");
      //���������ݽ���
      no_mqtt_msg_exchange = 0;
    }
  }
}

void Add_User_Threads()
{
  /* creat a new task to check if got IP */
  sys_thread_new("dhcpPollTask", dhcpPollTask, NULL, 256, osPriorityNormal);

  sys_thread_new("mqttSend", mqttSendTask, NULL, 1024, osPriorityNormal);
  sys_thread_new("mqttReceive", mqttReceiveTask, NULL, 1024, osPriorityNormal);
}
