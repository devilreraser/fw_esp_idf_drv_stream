/* *****************************************************************************
 * File:   drv_stream.c
 * Author: XX
 *
 * Created on YYYY MM DD
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "drv_stream.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_stream"

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */
#define DRV_STREAM_TRIGGER_LEVEL_BYTES   0   /* 0 is equal to 1 byte */

//#define DRV_STREAM_TICKS_TO_WAIT         pdMS_TO_TICKS(0)  
//#define DRV_STREAM_TICKS_TO_WAIT         pdMS_TO_TICKS(1)  
#define DRV_STREAM_TICKS_TO_WAIT         portMAX_DELAY

#define DRV_STREAM_USE_SEMAPHORE         1
#define DRV_STREAM_SEMAPHORE_TICKS       portMAX_DELAY

#define DRV_STREAM_DEFAULT_LENGTH_MAX    2048

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */
typedef struct 
{
    StreamBufferHandle_t* pStreamBuffer;
    SemaphoreHandle_t xStreamBufferMutex;
    char* pName;
} drv_stream_node_t;

/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */
drv_stream_node_t drv_stream_node_list[20] = {0};
int drv_stream_node_count = 0;

bool overwrite_old_data = false;        /* overwrite old data if no enough available space to write in buffer */
bool use_last_data_if_no_space = true;  /* true     - use last data when no overwrite_old_data and no enough space in buffer */
                                        /* false    - use first data when no overwrite_old_data and no enough space in buffer */

/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */
int drv_stream_node_find(StreamBufferHandle_t* pStreamBuffer)
{
    int index;
    for (index = 0; index < drv_stream_node_count; index++)
    {
        if (pStreamBuffer == drv_stream_node_list[index].pStreamBuffer)
        {
            break;
        }
    }
    return index;
}

void drv_stream_node_add(StreamBufferHandle_t* pStreamBuffer, char* pName)
{
    int index = drv_stream_node_find(pStreamBuffer);

    if (index >= drv_stream_node_count)
    {
        if (drv_stream_node_count < sizeof(drv_stream_node_list)/sizeof(drv_stream_node_list[0]))
        {
            index = drv_stream_node_count;
            drv_stream_node_count++;
        }
    }

    if (index < drv_stream_node_count)
    {
        drv_stream_node_list[index].pStreamBuffer = pStreamBuffer;
        #if DRV_STREAM_USE_SEMAPHORE
        drv_stream_node_list[index].xStreamBufferMutex = xSemaphoreCreateMutex();
        #endif
        drv_stream_node_list[index].pName = pName;
    }
}

void drv_stream_init(StreamBufferHandle_t* pStreamBuffer, size_t nStreamBufferSize, char* pName)
{
    if (nStreamBufferSize == 0)
    {
        nStreamBufferSize = DRV_STREAM_DEFAULT_LENGTH_MAX;
    }
    *pStreamBuffer = xStreamBufferCreate(nStreamBufferSize, DRV_STREAM_TRIGGER_LEVEL_BYTES);

    drv_stream_node_add(pStreamBuffer, pName);
}

void drv_stream_zero(StreamBufferHandle_t* pStreamBuffer)
{

    if (pStreamBuffer != NULL)
    {

        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X *pStreamBuffer @ %08X", __func__, (int)pStreamBuffer, (int)*pStreamBuffer);
        if (*pStreamBuffer != NULL)
        {
            int index = drv_stream_node_find(pStreamBuffer);
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreTake(drv_stream_node_list[index].xStreamBufferMutex, DRV_STREAM_SEMAPHORE_TICKS);
            }
            #endif

            int bytes_to_remove = xStreamBufferBytesAvailable(*pStreamBuffer);
            if (bytes_to_remove > 0)
            {
                uint8_t* pData = malloc(bytes_to_remove);
                if (pData)
                {
                    xStreamBufferReceive(*pStreamBuffer, pData, bytes_to_remove, DRV_STREAM_TICKS_TO_WAIT);
                    free(pData);
                }
                else
                {
                    ESP_LOGE(TAG, "Cannot Allocate Memory to free %d bytes in %s StreamBuffer", bytes_to_remove, drv_stream_node_list[index].pName);
                }

            }
            //xStreamBufferReset(*pStreamBuffer);
            //int size = xStreamBufferBytesAvailable(*pStreamBuffer) + xStreamBufferSpacesAvailable(*pStreamBuffer);
            //vStreamBufferDelete(*pStreamBuffer);
            //*pStreamBuffer = xStreamBufferCreate(size, DRV_STREAM_TRIGGER_LEVEL_BYTES);

            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreGive(drv_stream_node_list[index].xStreamBufferMutex);
            }
            #endif
        }

    }
    else
    {
        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X", __func__, (int)pStreamBuffer);
    }
        
}


//to do use smaller malloc and option to overwrite oldest data or not
size_t drv_stream_push(StreamBufferHandle_t* pStreamBuffer, uint8_t* pData, size_t nSize)
{
    if (pStreamBuffer != NULL)
    {

        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X *pStreamBuffer @ %08X", __func__, (int)pStreamBuffer, (int)*pStreamBuffer);
        if (*pStreamBuffer != NULL)
        {
            int index = drv_stream_node_find(pStreamBuffer);
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreTake(drv_stream_node_list[index].xStreamBufferMutex, DRV_STREAM_SEMAPHORE_TICKS);
            }
            #endif


            int bytes_removed_total = 0;
            int bytes_free = 0;
            size_t nSizeSendRequest = nSize;
            size_t nSizeSentTotal = 0;
            do
            {
                int bytes_to_remove = 0;
                int bytes_removed = 0;

                if (overwrite_old_data)
                {
                    bytes_free = xStreamBufferSpacesAvailable(*pStreamBuffer);
                    if (nSize > bytes_free)
                    {
                        bytes_to_remove = nSize - bytes_free;

                        uint8_t* pDataRemove = malloc(bytes_to_remove);
                        if (pDataRemove)
                        {
                            bytes_removed = xStreamBufferReceive(*pStreamBuffer, pDataRemove, bytes_to_remove, DRV_STREAM_TICKS_TO_WAIT);
                            free(pDataRemove);
                        }
                        else
                        {
                            ESP_LOGE(TAG, "Cannot Allocate Memory to free %d bytes in %s StreamBuffer", bytes_to_remove, drv_stream_node_list[index].pName);
                        }
                    }
                    
                }

                bytes_removed_total += bytes_removed;
                bytes_free = xStreamBufferSpacesAvailable(*pStreamBuffer);

                int bytes_to_send = nSize;

                if (bytes_to_send > bytes_free)
                {
                    bytes_to_send = bytes_free;
                }

                if (bytes_to_send > 0)
                {
                    if (use_last_data_if_no_space)
                    {
                        int bytes_skipped = nSize - bytes_to_send;
                        bytes_removed_total += bytes_skipped;
                        pData += bytes_skipped;
                        nSize -= bytes_skipped;

                    }

                    int bytes_sent = xStreamBufferSend(*pStreamBuffer, pData, bytes_to_send, DRV_STREAM_TICKS_TO_WAIT);

                    pData += bytes_sent;

                    nSize -= bytes_sent;
                }
                else
                {
                    bytes_removed_total += nSize;
                    nSize = 0;
                }


            } while (nSize > 0);
            
            nSizeSentTotal = nSizeSendRequest - bytes_removed_total;

            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreGive(drv_stream_node_list[index].xStreamBufferMutex);
            }
            #endif

            return nSizeSentTotal;


        }
        else
        {
            return 0;
        }

    }
    else
    {
        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X", __func__, (int)pStreamBuffer);
        return 0;
    }
}

size_t drv_stream_pull(StreamBufferHandle_t* pStreamBuffer, uint8_t* pData, size_t nSize)
{
    if (pStreamBuffer != NULL)
    {

        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X *pStreamBuffer @ %08X", __func__, (int)pStreamBuffer, (int)*pStreamBuffer);
        if (*pStreamBuffer != NULL)
        {
            int index = drv_stream_node_find(pStreamBuffer);
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreTake(drv_stream_node_list[index].xStreamBufferMutex, DRV_STREAM_SEMAPHORE_TICKS);
            }
            #endif
            

            int nSizeAvailable = xStreamBufferBytesAvailable(*pStreamBuffer);

            if (nSizeAvailable > nSize)
            {
                nSizeAvailable = nSize;
            }
            if (nSizeAvailable)
            {
                nSize = xStreamBufferReceive(*pStreamBuffer, pData, nSizeAvailable, DRV_STREAM_TICKS_TO_WAIT);
            }
            else
            {
                nSize = 0;
            }
            

            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreGive(drv_stream_node_list[index].xStreamBufferMutex);
            }
            #endif
            return nSize;

        }
        else
        {
            return 0;
        }

    }
    else
    {
        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X", __func__, (int)pStreamBuffer);
        return 0;
    }
}

int drv_stream_get_size(StreamBufferHandle_t* pStreamBuffer)
{
    if (pStreamBuffer != NULL)
    {

        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X *pStreamBuffer @ %08X", __func__, (int)pStreamBuffer, (int)*pStreamBuffer);
        if (*pStreamBuffer != NULL)
        {
            int index = drv_stream_node_find(pStreamBuffer);
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreTake(drv_stream_node_list[index].xStreamBufferMutex, DRV_STREAM_SEMAPHORE_TICKS);
            }
            #endif

            int nSize = xStreamBufferBytesAvailable(*pStreamBuffer);

            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreGive(drv_stream_node_list[index].xStreamBufferMutex);
            }
            #endif
            return nSize;

        }
        else
        {
            return 0;
        }

    }
    else
    {
        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X", __func__, (int)pStreamBuffer);
        return 0;
    }
}

int drv_stream_get_free(StreamBufferHandle_t* pStreamBuffer)
{
    if (pStreamBuffer != NULL)
    {

        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X *pStreamBuffer @ %08X", __func__, (int)pStreamBuffer, (int)*pStreamBuffer);
        if (*pStreamBuffer != NULL)
        {
            int index = drv_stream_node_find(pStreamBuffer);
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreTake(drv_stream_node_list[index].xStreamBufferMutex, DRV_STREAM_SEMAPHORE_TICKS);
            }
            #endif

            int nSize = xStreamBufferSpacesAvailable(*pStreamBuffer);
            
            #if DRV_STREAM_USE_SEMAPHORE
            if (drv_stream_node_list[index].xStreamBufferMutex)
            {
                xSemaphoreGive(drv_stream_node_list[index].xStreamBufferMutex);
            }
            #endif
            return nSize;

        }
        else
        {
            return 0;
        }

    }
    else
    {
        //ESP_LOGE(TAG, "%s : pStreamBuffer @ %08X", __func__, (int)pStreamBuffer);
        return 0;
    }
}
