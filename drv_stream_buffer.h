/* *****************************************************************************
 * File:   drv_stream_buffer.h
 * Author: XX
 *
 * Created on YYYY MM DD
 * 
 * Description: ...
 * 
 **************************************************************************** */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"
    
/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macro
 **************************************************************************** */

/* *****************************************************************************
 * Variables External Usage
 **************************************************************************** */ 

/* *****************************************************************************
 * Function Prototypes
 **************************************************************************** */
void drv_stream_buffer_init(StreamBufferHandle_t* pStreamBuffer, size_t nStreamBufferSize, char* pName);
void drv_stream_buffer_zero(StreamBufferHandle_t* pStreamBuffer);
size_t drv_stream_buffer_push(StreamBufferHandle_t* pStreamBuffer, uint8_t* pData, size_t nSize);
size_t drv_stream_buffer_pull(StreamBufferHandle_t* pStreamBuffer, uint8_t* pData, size_t nSize);
int drv_stream_buffer_get_size(StreamBufferHandle_t* pStreamBuffer);
int drv_stream_buffer_get_free(StreamBufferHandle_t* pStreamBuffer);

#ifdef __cplusplus
}
#endif /* __cplusplus */


