/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "url.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Returns whether the given character is a character that need not be
 * escaped when included as part of a component of a URL.
 *
 * @param c
 *     The character to test.
 *
 * @return
 *     Zero if the character does not need to be escaped when included as
 *     part of a component of a URL, non-zero otherwise.
 */
static int guac_kubernetes_is_url_safe(char c) {
    return (c >= 'A' && c <= 'Z')
        || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9')
        || strchr("-_.!~*'()", c) != NULL;
}

int guac_kubernetes_escape_url_component(char* output, int length,
        const char* str) {

    char* current = output;
    while (*str != '\0') {

        char c = *str;

        /* Store alphanumeric characters verbatim */
        if (guac_kubernetes_is_url_safe(c)) {

            /* Verify space exists for single character */
            if (length < 1)
                return 1;

            *(current++) = c;
            length--;

        }

        /* Escape EVERYTHING else as hex */
        else {

            /* Verify space exists for hex-encoded character */
            if (length < 4)
                return 1;

            snprintf(current, 4, "%%%02X", (int) c);

            current += 3;
            length -= 3;
        }

        /* Next character */
        str++;

    }

    /* Verify space exists for null terminator */
    if (length < 1)
        return 1;

    /* Append null terminator */
    *current = '\0';
    return 0;

}

int guac_kubernetes_append_endpoint_param(char* buffer, int length, 
        const char* param_name, const char* param_value) {

    char escaped_param_value[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];

    /* Escape value */
    if (guac_kubernetes_escape_url_component(escaped_param_value,
                    sizeof(escaped_param_value), param_value))
            return 1;
    
    char* str = buffer;

    int str_len = 0;
    int qmark = 0;

    while (*str != '\0') {

        /* Look for a question mark */
        if (*str=='?') qmark = 1;

        /* Compute the buffer string length */
        str_len++;

        /* Verify the buffer null terminated */
        if (str_len >= length) return 1;

        /* Next character */
        str++;
    }

    /* Determine the parameter delimiter */
    char delimiter = '?';
    if (qmark) delimiter = '&';

    /* Write the parameter to the buffer */
    int written;
    written = snprintf(buffer + str_len, length - str_len,
            "%c%s=%s", delimiter, param_name, escaped_param_value);

    /* The parameter was successfully added if it was written to the given
     * buffer without truncation */
    return (written < 0 || written >= length);
}

int guac_kubernetes_endpoint_uri(char* buffer, int length,
        const char* kubernetes_namespace, const char* kubernetes_pod,
        const char* kubernetes_container, const char* exec_command) {

    int written;

    char escaped_namespace[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];
    char escaped_pod[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];
    char escaped_container[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];
    char escaped_exec_command[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];

    /* Escape Kubernetes namespace */
    if (guac_kubernetes_escape_url_component(escaped_namespace,
                sizeof(escaped_namespace), kubernetes_namespace))
        return 1;

    /* Escape name of Kubernetes pod */
    if (guac_kubernetes_escape_url_component(escaped_pod,
                sizeof(escaped_pod), kubernetes_pod))
        return 1;

    /* Generate endpoint path depending on the call type */
    char* call = "attach";
    if (exec_command != NULL)
        call = "exec";

    char endpoint_path[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];

    written = snprintf(endpoint_path, sizeof(endpoint_path), 
        "/api/v1/namespaces/%s/pods/%s/%s", escaped_namespace, escaped_pod, call);
        
    if (written < 0 || written >= sizeof(endpoint_path))
        return 1;

    /* Generate endpoint params */
    char endpoint_params[GUAC_KUBERNETES_MAX_ENDPOINT_LENGTH];
    endpoint_params[0] = 0;

    if (exec_command != NULL) {
        /* Append exec command param */
        if (guac_kubernetes_append_endpoint_param(endpoint_params, 
                    sizeof(endpoint_params), "command", exec_command))
            return 1;
    }

    if (kubernetes_container != NULL) {
        /* Append kubernetes container param */
        if (guac_kubernetes_append_endpoint_param(endpoint_params, 
                    sizeof(endpoint_params), "container", kubernetes_container))
            return 1;
    }

    /* Combine path and params to uri */
    written = snprintf(buffer, length, "%s?%sstdin=true&stdout=true&tty=true", 
        endpoint_path, endpoint_params);
        
    /* Endpoint URL was successfully generated if it was written to the given
     * buffer without truncation */
    return !(written < length - 1);

}

