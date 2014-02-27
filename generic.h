/*
 * generic.h
 *
 *  Created on: Nov 8, 2012
 *      Author: shachindra
 */

#ifndef GENERIC_H_
#define GENERIC_H_

#define MAX_LENGTH_FILENAME              255
#define MAX_LENGTH_HOSTID                 10
#define MAX_LENGTH_ROUTERID               10
#define MAX_LENGTH_GROUP                  10
#define MAX_CONFIGFILE_TUPLE_LENGTH      255
#define MAX_PORTNUM_LENGTH                10
#define MAX_HOSTNAME_LENGTH              255

#define MAX_BUF_SIZE                     255

#define MAX_NUM_ROUTERS                   10
#define MAX_NUM_HOSTS                    100
#define MAX_NUM_HOSTS_PER_ROUTER          10
#define MAX_NUM_MULTICAST_GROUPS         100

#define MAX_PENDING                       10

#define INF                               99

void dijkstra(int cost[][MAX_NUM_ROUTERS], int no_of_routers, int source,int target, int* route);

#endif /* GENERIC_H_ */
