/*
 * system.c
 *
 *  Created on: Oct 19, 2017
 *      Author: zulolo
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hiredis.h"
#include "system.h"

#define REDIS_HOST_NAME			"127.0.0.1"
#define REDIS_HOST_PORT			6379
#define REDIS_MAX_KEY_LEN		50
#define REDIS_MAX_VALUE_LEN		50

int32_t nSetSystemValue(char* pKey, char* pValue) {
    redisContext *pRedisContext;
    redisReply *pRedisReply;

	if ((strnlen(pKey, REDIS_MAX_KEY_LEN) == REDIS_MAX_KEY_LEN)
			|| (strnlen(pValue, REDIS_MAX_VALUE_LEN) == REDIS_MAX_VALUE_LEN)) {
		REMOTE_CAR_LOG_ERR(
				"System info can not accept key length longer than %d or value length longer than %d.",
				REDIS_MAX_KEY_LEN, REDIS_MAX_VALUE_LEN);
		return (-1);
	}

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    pRedisContext = redisConnectWithTimeout(REDIS_HOST_NAME, REDIS_HOST_PORT, timeout);
    if (pRedisContext == NULL || pRedisContext->err) {
        if (pRedisContext) {
        	REMOTE_CAR_LOG_ERR("Redis connection error: %s.", pRedisContext->errstr);
            redisFree(pRedisContext);
        } else {
        	REMOTE_CAR_LOG_ERR("Redis connection error: can't allocate redis context.");
        }
        return(-1);
    }

    /* Set a key */
    pRedisReply = redisCommand(pRedisContext,"SET %s %s", pKey, pValue);
//    REMOTE_CAR_LOG_INFO("Redis SET: %s.", pRedisReply->str);
    freeReplyObject(pRedisReply);

    /* Disconnects and frees the context */
    redisFree(pRedisContext);
	return 0;
}

int32_t nClearSystemValue(char* pKey) {
    redisContext *pRedisContext;
    redisReply *pRedisReply;

	if (strnlen(pKey, REDIS_MAX_KEY_LEN) == REDIS_MAX_KEY_LEN) {
		REMOTE_CAR_LOG_ERR(
				"System info can not accept key length longer than %d.",
				REDIS_MAX_KEY_LEN);
		return (-1);
	}

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    pRedisContext = redisConnectWithTimeout(REDIS_HOST_NAME, REDIS_HOST_PORT, timeout);
    if (pRedisContext == NULL || pRedisContext->err) {
        if (pRedisContext) {
        	REMOTE_CAR_LOG_ERR("Redis connection error: %s.", pRedisContext->errstr);
            redisFree(pRedisContext);
        } else {
        	REMOTE_CAR_LOG_ERR("Redis connection error: can't allocate redis context.");
        }
        return(-1);
    }

    /* Set a key */
    pRedisReply = redisCommand(pRedisContext, "DEL %s", pKey);
    freeReplyObject(pRedisReply);

    /* Disconnects and frees the context */
    redisFree(pRedisContext);
	return 0;

}

int32_t nIncrSystemValue(char* pKey) {
    redisContext *pRedisContext;
    redisReply *pRedisReply;

	if (strnlen(pKey, REDIS_MAX_KEY_LEN) == REDIS_MAX_KEY_LEN) {
		REMOTE_CAR_LOG_ERR(
				"System info can not accept key length longer than %d.",
				REDIS_MAX_KEY_LEN);
		return (-1);
	}

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds
    pRedisContext = redisConnectWithTimeout(REDIS_HOST_NAME, REDIS_HOST_PORT, timeout);
    if (pRedisContext == NULL || pRedisContext->err) {
        if (pRedisContext) {
        	REMOTE_CAR_LOG_ERR("Redis connection error: %s.", pRedisContext->errstr);
            redisFree(pRedisContext);
        } else {
        	REMOTE_CAR_LOG_ERR("Redis connection error: can't allocate redis context.");
        }
        return(-1);
    }

    /* Set a key */
    pRedisReply = redisCommand(pRedisContext, "INCR %s", pKey);
    freeReplyObject(pRedisReply);

    /* Disconnects and frees the context */
    redisFree(pRedisContext);
	return 0;
}

