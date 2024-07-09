/**
 ******************************************************************************
 * @file    WZTOE/WZTOE_WebServer/main.c
 * @author  WIZnet
 * @brief   Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; COPYRIGHT 2018 WIZnet</center></h2>
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "wizchip_conf.h"
#include "dhcp.h"

/** @addtogroup W7500x_StdPeriph_Examples
 * @{
 */

/** @addtogroup WZTOE_WebServer
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define DATA_BUF_SIZE 2048

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static __IO uint32_t TimingDelay;
uint8_t test_buf[DATA_BUF_SIZE];
wiz_NetInfo gWIZNETINFO;

/* Private function prototypes -----------------------------------------------*/
static void UART_Config(void);
static void GPIO_Config(void);
static void DUALTIMER_Config(void);
static void Network_Config(void);
void dhcp_assign(void);
void dhcp_update(void);
void dhcp_conflict(void);
int32_t WebServer(uint8_t sn, uint8_t* buf, uint16_t port);
void delay(__IO uint32_t milliseconds);
void TimingDelay_Decrement(void);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Main program.
 * @param  None
 * @retval None
 */
int main(void)
{
    uint32_t ret;
    uint8_t dhcp_retry = 0;

    SystemInit();

    /* SysTick_Config */
    SysTick_Config((GetSystemClock() / 1000));

    /* Set WZ_100US Register */
    setTIC100US((GetSystemClock() / 10000));

    UART_Config();
    GPIO_Config();
    DUALTIMER_Config();

    printf("W7500x Standard Peripheral Library version : %d.%d.%d\r\n", __W7500X_STDPERIPH_VERSION_MAIN, __W7500X_STDPERIPH_VERSION_SUB1, __W7500X_STDPERIPH_VERSION_SUB2);

    printf("SourceClock : %d\r\n", (int) GetSourceClock());
    printf("SystemClock : %d\r\n", (int) GetSystemClock());

    /* Initialize PHY */
#ifdef W7500
    printf("PHY Init : %s\r\n", PHY_Init(GPIOB, GPIO_Pin_15, GPIO_Pin_14) == SET ? "Success" : "Fail");
#elif defined (W7500P)
    printf("PHY Init : %s\r\n", PHY_Init(GPIOB, GPIO_Pin_14, GPIO_Pin_15) == SET ? "Success" : "Fail");
#endif

    /* Check Link */
    printf("Link : %s\r\n", PHY_GetLinkStatus() == PHY_LINK_ON ? "On" : "Off");

    /* Network information setting before DHCP operation. Set only MAC. */
    Network_Config();

    /* DHCP Process */
    DHCP_init(0, test_buf);
    reg_dhcp_cbfunc(dhcp_assign, dhcp_update, dhcp_conflict);
    if (gWIZNETINFO.dhcp == NETINFO_DHCP) {       // DHCP
        printf("Start DHCP\r\n");
        while (1) {
            ret = DHCP_run();

            if (ret == DHCP_IP_LEASED) {
                printf("DHCP Success\r\n");
                break;
            }
            else if (ret == DHCP_FAILED) {
                dhcp_retry++;
            }

            if (dhcp_retry > 3) {
                printf("DHCP Fail\r\n");
                break;
            }
        }
    }

    /* Network information setting after DHCP operation.
     * Displays the network information allocated by DHCP. */
    Network_Config();

    printf("System Loop Start\r\n");

    while (1) {
        WebServer(1, test_buf, 80);
    }
	
	return 0;
}

/**
 * @brief  Configures the UART Peripheral.
 * @note
 * @param  None
 * @retval None
 */
static void UART_Config(void)
{
    UART_InitTypeDef UART_InitStructure;

    UART_StructInit(&UART_InitStructure);

#if defined (USE_WIZWIKI_W7500_EVAL)
    UART_Init(UART1, &UART_InitStructure);
    UART_Cmd(UART1, ENABLE);
#else
    S_UART_Init(115200);
    S_UART_Cmd(ENABLE);
#endif
}

/**
 * @brief  Configures the GPIO Peripheral.
 * @note   GPIO pin configures for ADC
 * @param  None
 * @retval None
 */
static void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Direction = GPIO_Direction_IN;
    GPIO_InitStructure.GPIO_Pad = GPIO_Pad_Default;
    GPIO_InitStructure.GPIO_AF = PAD_AF0;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

/**
 * @brief  Configures the DUALTIMER Peripheral.
 * @note
 * @param  None
 * @retval None
 */
static void DUALTIMER_Config(void)
{
    DUALTIMER_InitTypDef DUALTIMER_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    DUALTIMER_InitStructure.Timer_Load = GetSystemClock() / 1; //1s
    DUALTIMER_InitStructure.Timer_Prescaler = DUALTIMER_Prescaler_1;
    DUALTIMER_InitStructure.Timer_Wrapping = DUALTIMER_Periodic;
    DUALTIMER_InitStructure.Timer_Repetition = DUALTIMER_Wrapping;
    DUALTIMER_InitStructure.Timer_Size = DUALTIMER_Size_32;
    DUALTIMER_Init(DUALTIMER0_0, &DUALTIMER_InitStructure);

    DUALTIMER_ITConfig(DUALTIMER0_0, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DUALTIMER0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    DUALTIMER_Cmd(DUALTIMER0_0, ENABLE);
}

/**
 * @brief  Configures the Network Information.
 * @note
 * @param  None
 * @retval None
 */
static void Network_Config(void)
{
    uint8_t mac_addr[6] = { 0x00, 0x08, 0xDC, 0x01, 0x02, 0x03 };

    memcpy(gWIZNETINFO.mac, mac_addr, 6);
    gWIZNETINFO.dhcp = NETINFO_DHCP;

    ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);

    printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", gWIZNETINFO.mac[0], gWIZNETINFO.mac[1], gWIZNETINFO.mac[2], gWIZNETINFO.mac[3], gWIZNETINFO.mac[4], gWIZNETINFO.mac[5]);
    printf("IP: %d.%d.%d.%d\r\n", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1], gWIZNETINFO.ip[2], gWIZNETINFO.ip[3]);
    printf("GW: %d.%d.%d.%d\r\n", gWIZNETINFO.gw[0], gWIZNETINFO.gw[1], gWIZNETINFO.gw[2], gWIZNETINFO.gw[3]);
    printf("SN: %d.%d.%d.%d\r\n", gWIZNETINFO.sn[0], gWIZNETINFO.sn[1], gWIZNETINFO.sn[2], gWIZNETINFO.sn[3]);
    printf("DNS: %d.%d.%d.%d\r\n", gWIZNETINFO.dns[0], gWIZNETINFO.dns[1], gWIZNETINFO.dns[2], gWIZNETINFO.dns[3]);
}

/**
 * @brief  The call back function of ip assign.
 * @note
 * @param  None
 * @retval None
 */
void dhcp_assign(void)
{
    getIPfromDHCP(gWIZNETINFO.ip);
    getGWfromDHCP(gWIZNETINFO.gw);
    getSNfromDHCP(gWIZNETINFO.sn);
    getDNSfromDHCP(gWIZNETINFO.dns);

    ctlnetwork(CN_SET_NETINFO, (void*) &gWIZNETINFO);
}

/**
 * @brief  The call back function of ip update.
 * @note
 * @param  None
 * @retval None
 */
void dhcp_update(void)
{
    ;
}

/**
 * @brief  The call back function of ip conflict.
 * @note
 * @param  None
 * @retval None
 */
void dhcp_conflict(void)
{
    ;
}

/**
 * @brief  WebServer example function.
 * @note
 * @param  sn: Socket number to use.
 * @param  buf: The buffer the socket will use.
 * @param  port: Socket port number to use.
 * @retval Success or Fail of configuration functions
 */
int32_t WebServer(uint8_t sn, uint8_t* buf, uint16_t port)
{
    uint8_t i;
    uint8_t adcChannelOffset = 2;
    int32_t ret;
    uint16_t size = 0;
    uint8_t destip[4];
    uint16_t destport;
    uint8_t adc_buf[128] = { '\0', };

    switch (getSn_SR(sn))
    {
        case SOCK_ESTABLISHED:

            if (getSn_IR(sn) & Sn_IR_CON) {

                getSn_DIPR(sn, destip);
                destport = getSn_DPORT(sn);
                printf("%d:Connected - %d.%d.%d.%d : %d\r\n", sn, destip[0], destip[1], destip[2], destip[3], destport);

                setSn_IR(sn, Sn_IR_CON);
            }

            if ((size = getSn_RX_RSR(sn)) > 0) {
                if (size > DATA_BUF_SIZE) size = DATA_BUF_SIZE;
                ret = recv(sn, buf, size);
                if (ret <= 0) return ret;
                printf("%s", buf);
                             
                // Read the ADC value (mock example here)
                adcChannelOffset = 2;
                ADC_ChannelConfig(i+adcChannelOffset);
                ADC_StartOfConversion();
                int adc_value = ADC_GetConversionValue();
                ADC_Cmd(DISABLE);

                ret = send(sn, "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Connection: close\r\n"
                        "Refresh: 5\r\n"
                        "\r\n"
                        "<!DOCTYPE HTML>\r\n"
                        "<html>\r\n"
                        "<head>\r\n"
                        "    <meta charset=\"UTF-8\">\r\n"
                        "    <meta http-equiv=\"refresh\" content=\"5\">\r\n"
                        "    <title>Wiznet W7500x Web Server</title>\r\n"
                        "    <style>\r\n"
                        "        body {\r\n"
                        "            font-family: Arial, sans-serif;\r\n"
                        "            margin: 0;\r\n"
                        "            padding: 0;\r\n"
                        "            display: flex;\r\n"
                        "            justify-content: center;\r\n"
                        "            align-items: center;\r\n"
                        "            height: 100vh;\r\n"
                        "            background-color: #f4f4f9;\r\n"
                        "        }\r\n"
                        "        .container {\r\n"
                        "            text-align: center;\r\n"
                        "            background: #fff;\r\n"
                        "            padding: 20px;\r\n"
                        "            border-radius: 10px;\r\n"
                        "            box-shadow: 0 4px 8px rgba(0,0,0,0.1);\r\n"
                        "        }\r\n"
                        "        h1 {\r\n"
                        "            color: #333;\r\n"
                        "        }\r\n"
                        "        .gauge {\r\n"
                        "            width: 200px;\r\n"
                        "            height: 200px;\r\n"
                        "            border-radius: 50%;\r\n"
                        "            background: conic-gradient(#4caf50 0% 50%, #f44336 50% 100%);\r\n"
                        "            position: relative;\r\n"
                        "            margin: 20px auto;\r\n"
                        "        }\r\n"
                        "        .gauge:before {\r\n"
                        "            content: '';\r\n"
                        "            width: 160px;\r\n"
                        "            height: 160px;\r\n"
                        "            background: #fff;\r\n"
                        "            border-radius: 50%;\r\n"
                        "            position: absolute;\r\n"
                        "            top: 50%;\r\n"
                        "            left: 50%;\r\n"
                        "            transform: translate(-50%, -50%);\r\n"
                        "        }\r\n"
                        "        .gauge:after {\r\n"
                        "            content: '", sizeof("HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Connection: close\r\n"
                        "Refresh: 5\r\n"
                        "\r\n"
                        "<!DOCTYPE HTML>\r\n"
                        "<html>\r\n"
                        "<head>\r\n"
                        "    <meta charset=\"UTF-8\">\r\n"
                        "    <meta http-equiv=\"refresh\" content=\"5\">\r\n"
                        "    <title>Wiznet W7500x Web Server</title>\r\n"
                        "    <style>\r\n"
                        "        body {\r\n"
                        "            font-family: Arial, sans-serif;\r\n"
                        "            margin: 0;\r\n"
                        "            padding: 0;\r\n"
                        "            display: flex;\r\n"
                        "            justify-content: center;\r\n"
                        "            align-items: center;\r\n"
                        "            height: 100vh;\r\n"
                        "            background-color: #f4f4f9;\r\n"
                        "        }\r\n"
                        "        .container {\r\n"
                        "            text-align: center;\r\n"
                        "            background: #fff;\r\n"
                        "            padding: 20px;\r\n"
                        "            border-radius: 10px;\r\n"
                        "            box-shadow: 0 4px 8px rgba(0,0,0,0.1);\r\n"
                        "        }\r\n"
                        "        h1 {\r\n"
                        "            color: #333;\r\n"
                        "        }\r\n"
                        "        .gauge {\r\n"
                        "            width: 200px;\r\n"
                        "            height: 200px;\r\n"
                        "            border-radius: 50%;\r\n"
                        "            background: conic-gradient(#4caf50 0% 50%, #f44336 50% 100%);\r\n"
                        "            position: relative;\r\n"
                        "            margin: 20px auto;\r\n"
                        "        }\r\n"
                        "        .gauge:before {\r\n"
                        "            content: '';\r\n"
                        "            width: 160px;\r\n"
                        "            height: 160px;\r\n"
                        "            background: #fff;\r\n"
                        "            border-radius: 50%;\r\n"
                        "            position: absolute;\r\n"
                        "            top: 50%;\r\n"
                        "            left: 50%;\r\n"
                        "            transform: translate(-50%, -50%);\r\n"
                        "        }\r\n"
                        "        .gauge:after {\r\n"
                        "            content: '") - 1);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                char value_str[4];
                snprintf(value_str, sizeof(value_str), "%d", adc_value);
                ret = send(sn, value_str, strlen(value_str));
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                ret = send(sn, "';\r\n"
                        "            font-size: 2em;\r\n"
                        "            color: #333;\r\n"
                        "            position: absolute;\r\n"
                        "            top: 50%;\r\n"
                        "            left: 50%;\r\n"
                        "            transform: translate(-50%, -50%);\r\n"
                        "        }\r\n"
                        "    </style>\r\n"
                        "</head>\r\n"
                        "<body>\r\n"
                        "    <div class=\"container\">\r\n"
                        "        <h1>Wiznet W7500x Web Server</h1>\r\n"
                        "        <div class=\"gauge\" style=\"background: conic-gradient(#4caf50 0% ", sizeof("';\r\n"
                        "            font-size: 2em;\r\n"
                        "            color: #333;\r\n"
                        "            position: absolute;\r\n"
                        "            top: 50%;\r\n"
                        "            left: 50%;\r\n"
                        "            transform: translate(-50%, -50%);\r\n"
                        "        }\r\n"
                        "    </style>\r\n"
                        "</head>\r\n"
                        "<body>\r\n"
                        "    <div class=\"container\">\r\n"
                        "        <h1>Wiznet W7500x Web Server</h1>\r\n"
                        "        <div class=\"gauge\" style=\"background: conic-gradient(#4caf50 0% ") - 1);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                char gauge_value_str[4];
                snprintf(gauge_value_str, sizeof(gauge_value_str), "%d", adc_value / 2);
                ret = send(sn, gauge_value_str, strlen(gauge_value_str));
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                ret = send(sn, "%, #f44336 ", sizeof("%, #f44336 ") - 1);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                ret = send(sn, gauge_value_str, strlen(gauge_value_str));
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                ret = send(sn, "% 100%);\">\r\n"
                        "        </div>\r\n"
                        "        <p>Analog input 1 is ", sizeof("% 100%);\">\r\n"
                        "        </div>\r\n"
                        "        <p>Analog input 1 is ") - 1);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

   

                ret = send(sn, "</p>\r\n</div>\r\n</body>\r\n</html>\r\n", sizeof("</p>\r\n</div>\r\n</body>\r\n</html>\r\n") - 1);
                if (ret < 0) {
                    close(sn);
                    return ret;
                }

                disconnect(sn);
            }

            break;
        case SOCK_CLOSE_WAIT:

            if ((ret = disconnect(sn)) != SOCK_OK) return ret;

            printf("%d:Socket Closed\r\n", sn);

            break;
        case SOCK_INIT:

            printf("%d:Listen, Web server, port [%d]\r\n", sn, port);

            if ((ret = listen(sn)) != SOCK_OK) return ret;

            break;
        case SOCK_CLOSED:

            if ((ret = socket(sn, Sn_MR_TCP, port, 0x00)) != sn) return ret;

            break;
        default:
            break;
    }
    return 1;
}

/**
 * @brief  Inserts a delay time.
 * @param  nTime: specifies the delay time length, in milliseconds.
 * @retval None
 */
void delay(__IO uint32_t milliseconds)
{
    TimingDelay = milliseconds;

    while (TimingDelay != 0)
        ;
}

/**
 * @brief  Decrements the TimingDelay variable.
 * @param  None
 * @retval None
 */
void TimingDelay_Decrement(void)
{
    if (TimingDelay != 0x00) {
        TimingDelay--;
    }
}

#ifdef  USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

    /* Infinite loop */
    while (1)
    {
    }
}
#endif

/**
 * @}
 */

/**
 * @}
 */

/******************** (C) COPYRIGHT WIZnet *****END OF FILE********************/
