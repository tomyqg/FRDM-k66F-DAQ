#include "variables.h"
#include "debug.h"
#include "snmpConnect_manager.h"
#include "cJSON.h"

//network inclusions
#include "app_mqtt_client.h"
#include "core/net.h"
#include "mqtt/mqtt_client.h"
#include "yarrow.h"

// FreeRTOS inclustions
#include "os_port_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "mqtt_json_parse.h"
#include "mqtt_json_make.h"

#include <string.h>
#include <stdlib.h>
#include "mallocstats.h"

//Connection states
#define APP_STATE_NOT_CONNECTED 0
#define APP_STATE_CONNECTING    1
#define APP_STATE_CONNECTED     2

// Global variables
MqttClientContext mqttClientContext;
uint8_t mqttConnectionState = APP_STATE_NOT_CONNECTED;

cJSON* jsonMessage = NULL;
cJSON* jsonStatus = NULL;
cJSON* jsonName = NULL;

TaskHandle_t mqttMsgTask;
TaskHandle_t mqttPeriodicDataTask;
QueueHandle_t mqttRcvQueue;
QueueHandle_t mqttPubQueue;
char subscribeTopic[32];

#if APP_SERVER_PORT == 8883
/**
* @brief SSL/TLS initialization callback
* @param[in] context Pointer to the MQTT client context
* @param[in] tlsContext Pointer to the SSL/TLS context
* @return Error code
**/
error_t mqttTlsInitCallback(MqttClientContext *context,
                            TlsContext *tlsContext)
{
    error_t error;
    
    //Debug message
    TRACE_INFO("MQTT: TLS initialization callback\r\n");
    
    //Set the PRNG algorithm to be used
    error = tlsSetPrng(tlsContext, YARROW_PRNG_ALGO, &yarrowContext);
    //Any error to report?
    if(error)
        return error;
    
    //Set the fully qualified domain name of the server
    error = tlsSetServerName(tlsContext, APP_SERVER_NAME);
    //Any error to report?
    if(error)
        return error;
    
    //Import the list of trusted CA certificates
    error = tlsSetTrustedCaList(tlsContext, trustedCaList, strlen(trustedCaList));
    //Any error to report?
    if(error)
        return error;
    
    //Successful processing
    return NO_ERROR;
}

#endif //(APP_SERVER_PORT == 8883)
/**
* @brief Publish callback function
* @param[in] context Pointer to the MQTT client context
* @param[in] topic Topic name
* @param[in] message Message payload
* @param[in] length Length of the message payload
* @param[in] dup Duplicate delivery of the PUBLISH packet
* @param[in] qos QoS level used to publish the message
* @param[in] retain This flag specifies if the message is to be retained
* @param[in] packetId Packet identifier
**/

void mqttPublishCallback(MqttClientContext *context,
                         const char_t *topic, const uint8_t *message, size_t length,
                         bool_t dup, MqttQosLevel qos, bool_t retain, uint16_t packetId)
{
    mqtt_recv_buffer_t mqttBuffer;
        //Debug message
        TRACE_INFO("PUBLISH packet received...\r\n");
    TRACE_INFO("  Dup: %u\r\n", dup);
    TRACE_INFO("  QoS: %u\r\n", qos);
    TRACE_INFO("  Retain: %u\r\n", retain);
    TRACE_INFO("  Packet Identifier: %u\r\n", packetId);
    TRACE_INFO("  Topic: %s\r\n", topic);
    TRACE_INFO("  Message (%" PRIuSIZE " bytes):\r\n", length);
    if (length > MQTT_CLIENT_MSG_MAX_SIZE)
    {
        TRACE_INFO("received message's length is too large\r\n");
        return;
    }
    mqttBuffer.size = length;
    memset(mqttBuffer.message, 0, sizeof(mqttBuffer.message));
    memcpy(mqttBuffer.message, message, length);
    if (xQueueSend(mqttRcvQueue, &mqttBuffer, (TickType_t)100) != pdPASS)
    {
        TRACE_INFO("Can't send message to mqttRcvQueue\r\n");
    }
}

/* Publish message */
void mqttPublishMsg(char* topic, char* message, uint16_t msgSize)
{
    mqtt_pub_buffer_t mqttPubMsg;
    if ((strlen(topic) > MQTT_CLIENT_TOPIC_MAX_SIZE) || (msgSize > MQTT_CLIENT_MSG_MAX_SIZE) || (topic == NULL) || (message == NULL))
    {
        TRACE_INFO("publish message parameters invalid\r\n");
        return;
    }
    memset(&mqttPubMsg, 0, sizeof(mqtt_pub_buffer_t));
    memcpy(mqttPubMsg.topic, topic, strlen(topic));
    memcpy(mqttPubMsg.message, message, msgSize);
    if (xQueueSend(mqttPubQueue, &mqttPubMsg, (TickType_t)100) != pdPASS)
    {
        TRACE_INFO("Can't send message to mqttRcvQueue\r\n");
    }
}

/* periodically update data task */
void mqttPeriodicUpdateTask(void *param)
{
	char* message;
	while (1)
	{   
        
        if (mqttConnectionState == APP_STATE_CONNECTED)
        {            
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))
            __iar_dlmalloc_stats();
            TRACE_INFO("Make device info\r\n");
#endif
            message = mqtt_json_make_device_info(deviceName, &privateMibBase);
            mqttPublishMsg(MQTT_EVENT_TOPIC, message, strlen(message));
            if (message != NULL)
                free(message);
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))
            __iar_dlmalloc_stats();
            TRACE_INFO("Make ac phase\r\n");
#endif
            message = mqtt_json_make_ac_phase_info(deviceName, &privateMibBase);
            mqttPublishMsg(MQTT_EVENT_TOPIC, message, strlen(message));
            if (message != NULL)
                free(message);         
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))
            __iar_dlmalloc_stats();
            TRACE_INFO("Make battery\r\n");
#endif
            message = mqtt_json_make_battery_message(deviceName, &privateMibBase);
            mqttPublishMsg(MQTT_EVENT_TOPIC, message, strlen(message));
            if (message != NULL)
                free(message);
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))            
            __iar_dlmalloc_stats();
            TRACE_INFO("Make alarm\r\n");
#endif
            message = mqtt_json_make_alarm_message(deviceName, &privateMibBase);
            mqttPublishMsg(MQTT_EVENT_TOPIC, message, strlen(message));
            if (message != NULL)
                free(message);  
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))
            __iar_dlmalloc_stats();
            TRACE_INFO("Make accessories\r\n");
#endif
            message = mqtt_json_make_accessory_message(deviceName, &privateMibBase);
            mqttPublishMsg(MQTT_EVENT_TOPIC, message, strlen(message));
            if (message != NULL)
                free(message);	 
#if (defined(SDK_DEBUGCONSOLE) && (SDK_DEBUGCONSOLE==1))
            __iar_dlmalloc_stats();
            TRACE_INFO("End\r\n");
#endif
        }
        /* Check for OS heap memory use */
        TRACE_INFO("FreeRTOS free heap size: %d\r\n", (uint16_t)xPortGetFreeHeapSize());
        osDelayTask(30000);
	}
}

/* Handle MQTT message on topic DAQ/box_name */
void mqttMsgHandleTask(void * param)
{
    mqtt_recv_buffer_t mqttBuffer;
    char* responseMsg = NULL;
    mqttRcvQueue = xQueueCreate(MQTT_CLIENT_QUEUE_SIZE, sizeof(mqtt_recv_buffer_t));
    if (mqttRcvQueue == NULL)
    {
        TRACE_INFO("Can't create mqtt receive queue\r\n");
        vTaskDelete(NULL);
    }
    while(1)
    {
        if (xQueueReceive(mqttRcvQueue, &mqttBuffer, portMAX_DELAY) == pdTRUE)
        {
            TRACE_INFO("Message:\r\n%s\r\n", mqttBuffer.message);
            responseMsg = mqtt_json_parse_message(mqttBuffer.message, mqttBuffer.size);
            if (responseMsg != NULL)
            {
                TRACE_INFO ("response:\r\n%s\r\n", responseMsg);
                mqttPublishMsg(MQTT_RESPONSE_TOPIC, responseMsg, strlen(responseMsg));
                free(responseMsg);
            }
        }
    }
}

/**
* @brief Establish MQTT connection
**/
error_t mqttConnect(NetInterface *interface)
{
    error_t error;
    IpAddr ipAddr;
    MqttClientCallbacks mqttClientCallbacks;
    
    //Debug message
    TRACE_INFO("\r\n\r\nResolving server name...\r\n");
    
    //Resolve MQTT server name
    error = getHostByName(interface, APP_SERVER_NAME, &ipAddr, 0);
    //Any error to report?
    if(error)
        return error;
    
#if (APP_SERVER_PORT == 80 || APP_SERVER_PORT == 443)
    //Register RNG callback
    webSocketRegisterRandCallback(webSocketRngCallback);
#endif
    
    //Initialize MQTT client context
    mqttClientInit(&mqttClientContext);
    // set interface to connect with MQTT
    mqttClientContext.interface = interface;
    //Initialize MQTT client callbacks
    mqttClientInitCallbacks(&mqttClientCallbacks);
    
    //Attach application-specific callback functions
    mqttClientCallbacks.publishCallback = mqttPublishCallback;
#if (APP_SERVER_PORT == 8883 || APP_SERVER_PORT == 443)
    mqttClientCallbacks.tlsInitCallback = mqttTlsInitCallback;
#endif
    
    //Register MQTT client callbacks
    mqttClientRegisterCallbacks(&mqttClientContext, &mqttClientCallbacks);
    
    //Set the MQTT version to be used
    mqttClientSetProtocolLevel(&mqttClientContext,
                               MQTT_PROTOCOL_LEVEL_3_1_1);
    
#if (APP_SERVER_PORT == 1883)
    //MQTT over TCP
    mqttClientSetTransportProtocol(&mqttClientContext, MQTT_TRANSPORT_PROTOCOL_TCP);
#elif (APP_SERVER_PORT == 8883)
    //MQTT over SSL/TLS
    mqttClientSetTransportProtocol(&mqttClientContext, MQTT_TRANSPORT_PROTOCOL_TLS);
#elif (APP_SERVER_PORT == 80)
    //MQTT over WebSocket
    mqttClientSetTransportProtocol(&mqttClientContext, MQTT_TRANSPORT_PROTOCOL_WS);
#elif (APP_SERVER_PORT == 443)
    //MQTT over secure WebSocket
    mqttClientSetTransportProtocol(&mqttClientContext, MQTT_TRANSPORT_PROTOCOL_WSS);
#endif
    
    //Set keep-alive value
    mqttClientSetKeepAlive(&mqttClientContext, 30);
    
#if (APP_SERVER_PORT == 80 || APP_SERVER_PORT == 443)
    //Set the hostname of the resource being requested
    mqttClientSetHost(&mqttClientContext, APP_SERVER_NAME);
    //Set the name of the resource being requested
    mqttClientSetUri(&mqttClientContext, APP_SERVER_URI);
#endif
    //Set client identifier
    mqttClientSetIdentifier(&mqttClientContext, deviceName);
    
    //Set user name and password
    //mqttClientSetAuthInfo(&mqttClientContext, "username", "password");
    
    //Set Will message
//    mqttClientSetWillMessage(&mqttClientContext, "DAQ/event",
//                             "offline", 7, MQTT_QOS_LEVEL_0, FALSE);
    
    //Debug message
    TRACE_INFO("Connecting to MQTT server %s...\r\n", ipAddrToString(&ipAddr, NULL));
    
    //Start of exception handling block
    do
    {
        //Establish connection with the MQTT server
        error = mqttClientConnect(&mqttClientContext,
                                  &ipAddr, APP_SERVER_PORT, TRUE);
        //Any error to report?
        if(error)
            break;
        
        //Subscribe to the desired topics
        TRACE_INFO("Subscribe to topic: %s\r\n", subscribeTopic);
        error = mqttClientSubscribe(&mqttClientContext,
                                    subscribeTopic, MQTT_QOS_LEVEL_1, NULL);
        //Any error to report?
        if(error)
            break;
        
        //Send PUBLISH packet
        char *onlineMsg = mqtt_json_make_online_message(deviceName);
        if (onlineMsg != NULL)
        {
            error = mqttClientPublish(&mqttClientContext, MQTT_EVENT_TOPIC,
                                      onlineMsg, strlen(onlineMsg), MQTT_QOS_LEVEL_1, FALSE, NULL);
            free(onlineMsg);
            //Any error to report?
            if(error)
                break;
        }       
        //End of exception handling block
    } while(0);
    
    //Check status code
    if(error)
    {
        //Close connection
        mqttClientClose(&mqttClientContext);
    }
    
    //Return status code
    return error;
}

/* MQTT CLIENT MAIN TASK */
void mqttClientTask (void *param)
{
    error_t error;
    mqtt_pub_buffer_t mqttPubBuffer;
    sprintf(subscribeTopic, "DAQ/%s", deviceName);
    // Start receive handle task
    mqttPubQueue = xQueueCreate(MQTT_CLIENT_QUEUE_SIZE, sizeof(mqtt_pub_buffer_t));
    if (mqttPubQueue == NULL)
    {
        TRACE_INFO("Can't create publish queue\r\n");
        vTaskDelete(NULL);
    }
    if (xTaskCreate(mqttMsgHandleTask, "mqtt_handle_receive", MQTT_RECV_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &mqttMsgTask) != pdPASS)
        vTaskDelete(NULL);
    if (xTaskCreate(mqttPeriodicUpdateTask, "mqtt_periodic_data", MQTT_DATA_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY, &mqttPeriodicDataTask) != pdPASS)
        vTaskDelete(NULL);
    //Endless loop
    while(1)
    {
        //Check connection state
        if(mqttConnectionState == APP_STATE_NOT_CONNECTED)
        {
            //Update connection state
            mqttConnectionState = APP_STATE_CONNECTING;
            
            //Try to connect to the MQTT server using ppp interface
            if (interfaceManagerGetActiveInterface() != NULL)
                error = mqttConnect(interfaceManagerGetActiveInterface());
            else
                error = ERROR_FAILURE;
            
            //Failed to connect to the MQTT server?
            if(error)
            {
                //Update connection state
                mqttConnectionState = APP_STATE_NOT_CONNECTED;
                //Recovery delay
                osDelayTask(5000);
            }
            else
            {
                //Update connection state
                mqttConnectionState = APP_STATE_CONNECTED;
            }
        }
        else
        {
            //Process incoming events
            error = mqttClientProcessEvents(&mqttClientContext, 10);            
            //Connection to MQTT server lost?
            if(error != NO_ERROR && error != ERROR_TIMEOUT)
            {
                //Close connection
                mqttClientClose(&mqttClientContext);
                //Update connection state
                mqttConnectionState = APP_STATE_NOT_CONNECTED;
                //Recovery delay
                osDelayTask(2000);
            }
            else
            {
                error = NO_ERROR;
                if (xQueueReceive(mqttPubQueue, &mqttPubBuffer, OS_MS_TO_SYSTICKS(5)) == pdTRUE)
                {                    
                    error = mqttClientPublish(&mqttClientContext, mqttPubBuffer.topic,
                                              mqttPubBuffer.message, strlen(mqttPubBuffer.message), MQTT_QOS_LEVEL_1, FALSE, NULL);
                    //Failed to publish data?
                    if(error)
                    {
                        //Close connection
                        mqttClientClose(&mqttClientContext);
                        //Update connection state
                        mqttConnectionState = APP_STATE_NOT_CONNECTED;
                        //Recovery delay
                        osDelayTask(1000);
                    }
                }
#if (MQTT_SEND_TEST_MSG == ENABLED)
                //Initialize status code
                static unsigned int packetID = 1;
                //Send PUBLISH packet    
                jsonMessage = cJSON_CreateObject();
                jsonName = cJSON_CreateString(deviceName);
                cJSON_AddItemToObject(jsonMessage, "id", jsonName);
                jsonStatus = cJSON_CreateNumber(packetID);
                packetID++;
                cJSON_AddItemToObject(jsonMessage, "message_id", jsonStatus);
                char* publishMessage;
                publishMessage = cJSON_Print(jsonMessage);
                cJSON_Delete(jsonMessage);
                
                error = mqttClientPublish(&mqttClientContext, subscribeTopic,
                                          publishMessage, strlen(publishMessage), MQTT_QOS_LEVEL_2, FALSE, NULL);
                free(publishMessage);
                //Failed to publish data?
                if(error)
                {
                    //Close connection
                    mqttClientClose(&mqttClientContext);
                    //Update connection state
                    mqttConnectionState = APP_STATE_NOT_CONNECTED;
                    //Recovery delay
                    osDelayTask(10000);
                }
                osDelayTask(3000);
#endif            
            }
			// check if network interface changed then close for fast recovery
			if (mqttClientContext.interface != interfaceManagerGetActiveInterface())
			{
				 //Close connection
                mqttClientClose(&mqttClientContext);
                //Update connection state
                mqttConnectionState = APP_STATE_NOT_CONNECTED;
                //Recovery delay
				osDelayTask(2000);
			}			
        }
    }
}