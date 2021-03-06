/*
 * This file is part of Arakoon, a distributed key-value store.
 *
 * Copyright (C) 2010, 2012 Incubaid BVBA
 *
 * Licensees holding a valid Incubaid license may use this file in
 * accordance with Incubaid's Arakoon commercial license agreement. For
 * more information on how to enter into this agreement, please contact
 * Incubaid (contact details can be found on http://www.arakoon.org/licensing).
 *
 * Alternatively, this file may be redistributed and/or modified under
 * the terms of the GNU Affero General Public License version 3, as
 * published by the Free Software Foundation. Under this license, this
 * file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU Affero General Public License for more details.
 * You should have received a copy of the
 * GNU Affero General Public License along with this program (file "COPYING").
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "arakoon.h"
#include "arakoon-protocol.h"
#include "arakoon-utils.h"
#include "arakoon-cluster-node.h"
#include "arakoon-assert.h"
#include "arakoon-networking.h"

struct ArakoonClusterNode {
        char * name;
        const ArakoonCluster * cluster;
        struct addrinfo * address;
        int fd;

        ArakoonClusterNode * next;
};

ArakoonClusterNode * arakoon_cluster_node_new(const char * name) {
        ArakoonClusterNode *ret = NULL;
        size_t len = 0;

        FUNCTION_ENTER(arakoon_cluster_node_new);

        ret = arakoon_mem_new(1, ArakoonClusterNode);
        RETURN_NULL_IF_NULL(ret);

        memset(ret, 0, sizeof(ArakoonClusterNode));

        len = strlen(name) + 1;
        ret->name = arakoon_mem_new(len, char);
        if(ret->name == NULL) {
                goto nomem;
        }

        strncpy(ret->name, name, len);

        ret->cluster = NULL;
        ret->address = NULL;
        ret->fd = -1;
        ret->next = NULL;

        return ret;

nomem:
        if(ret != NULL) {
                arakoon_mem_free(ret->name);
        }

        arakoon_mem_free(ret);

        return NULL;
}

void arakoon_cluster_node_free(ArakoonClusterNode *node) {
        FUNCTION_ENTER(_arakoon_cluster_node_free);

        RETURN_IF_NULL(node);

        if(node->fd >= 0) {
                _arakoon_log_warning(
                        "arakoon-cluster-node: freeing a cluster node "
                        "which wasn't disconnected before");
                _arakoon_cluster_node_disconnect(node);
        }

        arakoon_mem_free(node->name);
        freeaddrinfo(node->address);
        arakoon_mem_free(node);
}

arakoon_rc _arakoon_cluster_node_connect(ArakoonClusterNode *node,
    int *timeout) {
        size_t n = 0, len = 0;
        char *prologue = NULL, *p = NULL;
        arakoon_rc rc = 0;
        const char *name = NULL;

        FUNCTION_ENTER(_arakoon_cluster_node_connect);

        _arakoon_log_info("arakoon-cluster-node: connecting to %s",
                node->name);

        if(node->fd >= 0) {
                _arakoon_log_warning(
                        "arakoon-cluster-node: arakoon_cluster_node_connect "
                        "called, but FD >= 0");

                return ARAKOON_RC_SUCCESS;
        }

        rc = _arakoon_networking_connect(node->address, &(node->fd), timeout);

        if(rc != ARAKOON_RC_SUCCESS) {
                node->fd = -1;

                _arakoon_log_error(
                        "arakoon-cluster-node: unable to connect to node %s",
                        node->name);

                return rc;
        }

        _arakoon_log_info("arakoon-cluster-node: connected to node %s, fd %d",
                node->name, node->fd);

        /* Send prologue */
        name = arakoon_cluster_get_name(node->cluster);
        n = strlen(name);
        len = ARAKOON_PROTOCOL_COMMAND_LEN + ARAKOON_PROTOCOL_INT32_LEN
                + ARAKOON_PROTOCOL_STRING_LEN(n);

        prologue = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(prologue);

        p = prologue;

        ARAKOON_PROTOCOL_WRITE_COMMAND(p, 0, 0);
        ARAKOON_PROTOCOL_WRITE_INT32(p, ARAKOON_PROTOCOL_VERSION);
        ARAKOON_PROTOCOL_WRITE_STRING(p, name, n);

        WRITE_BYTES(node, prologue, len, rc, timeout);

        arakoon_mem_free(prologue);

        return rc;
}

void _arakoon_cluster_node_disconnect(ArakoonClusterNode *node) {
        FUNCTION_ENTER(_arakoon_internal_cluster_node_disconnect);

        if(node->fd >= 0) {
                _arakoon_log_info(
                        "arakoon-cluster-node: disconnecting from node %s, fd %d",
                        node->name, node->fd);
                _arakoon_networking_shutdown_wrapper(node->fd, SHUT_RDWR);
                _arakoon_networking_close_wrapper(node->fd);
        }

        node->fd = -1;
}

arakoon_rc _arakoon_cluster_node_who_master(ArakoonClusterNode *node,
    int *timeout, char ** const master) {
        size_t len = 0;
        char *command = NULL, *c = NULL;
        arakoon_rc rc = 0;
        void *result_data = NULL;
        size_t result_size = 0;

        FUNCTION_ENTER(arakoon_cluster_node_who_master);

        len = ARAKOON_PROTOCOL_COMMAND_LEN;

        command = arakoon_mem_new(len, char);
        RETURN_ENOMEM_IF_NULL(command);

        c = command;

        ARAKOON_PROTOCOL_WRITE_COMMAND(c, 0x02, 0x00);

        ASSERT_ALL_WRITTEN(command, c, len);

        WRITE_BYTES(node, command, len, rc, timeout);
        arakoon_mem_free(command);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_RC(node, rc, timeout);
        RETURN_IF_NOT_SUCCESS(rc);

        ARAKOON_PROTOCOL_READ_STRING_OPTION(node, result_data, result_size,
                rc, timeout);
        if(!ARAKOON_RC_IS_SUCCESS(rc)) {
                *master = NULL;
                return rc;
        }

        if(result_data == NULL) {
                *master = NULL;
        }
        else {
                *master = arakoon_utils_make_string(result_data, result_size);
                RETURN_ENOMEM_IF_NULL(*master);
        }

        return rc;
}

const char * _arakoon_cluster_node_get_name(const ArakoonClusterNode * const node) {
        return node->name;
}

int _arakoon_cluster_node_get_fd(const ArakoonClusterNode * const node) {
        return node->fd;
}

ArakoonClusterNode * _arakoon_cluster_node_get_next(
    const ArakoonClusterNode * const node) {
        return node->next;
}

void _arakoon_cluster_node_set_next(ArakoonClusterNode *node,
    ArakoonClusterNode *next) {
        node->next = next;
}

arakoon_rc _arakoon_cluster_node_write_bytes(ArakoonClusterNode *node,
    size_t len, void *data, int *timeout) {
        return _arakoon_networking_poll_write(node->fd, data, len, timeout);
}

arakoon_rc arakoon_cluster_node_add_address(ArakoonClusterNode *node,
    struct addrinfo *address) {
        struct addrinfo *rp = NULL;

        FUNCTION_ENTER(arakoon_cluster_node_add_address);

        ASSERT_NON_NULL_RC(node);
        ASSERT_NON_NULL_RC(address);

        if(node->address == NULL) {
                node->address = address;
        }
        else {
                for(rp = node->address; rp->ai_next != NULL; rp = rp->ai_next);

                if(rp->ai_next != NULL) {
                        abort();
                }

                rp->ai_next = address;
        }

        return ARAKOON_RC_SUCCESS;
}

arakoon_rc arakoon_cluster_node_add_address_tcp(ArakoonClusterNode *node,
    const char * const host, const char * const service) {
        struct addrinfo hints;
        struct addrinfo *result;
        int rc = 0;

        FUNCTION_ENTER(arakoon_cluster_node_add_address_tcp);

        ASSERT_NON_NULL_RC(node);
        ASSERT_NON_NULL_RC(host);
        ASSERT_NON_NULL_RC(service);

        _arakoon_log_debug("arakoon-cluster-node: looking up node %s at %s:%s",
                _arakoon_cluster_node_get_name(node), host, service);

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC; /* IPv4 and IPv6, whatever */
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = 0;
        hints.ai_protocol = 0; /* Any protocol */

        rc = getaddrinfo(host, service, &hints, &result);

        if(rc != 0) {
                _arakoon_log_error(
                        "arakoon-cluster-node: address lookup failed: %s",
                        gai_strerror(rc));
                return ARAKOON_RC_CLIENT_NETWORK_ERROR;
        }

        rc = arakoon_cluster_node_add_address(node, result);

        return rc;
}

arakoon_rc _arakoon_cluster_node_set_cluster(ArakoonClusterNode *node,
    ArakoonCluster *cluster) {
        if(node->cluster != NULL) {
                return -EINVAL;
        }

        node->cluster = cluster;

        return ARAKOON_RC_SUCCESS;
}
