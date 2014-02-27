/*
 * generic.c
 *
 *  Created on: Nov 21, 2012
 *      Author: shachindra
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include "generic.h"

//in  - cost matrix, no_of_routers, source, target, array to hold the route of the shortest path
//out - total cost of shortest path

void dijkstra(int cost[][MAX_NUM_ROUTERS], int no_of_routers, int source,int target, int* route)
{
    FILE* routes = fopen("tempDijkstra.txt","w+");

    int dist[MAX_NUM_ROUTERS + 1],prev[MAX_NUM_ROUTERS + 1],selected[MAX_NUM_ROUTERS + 1]={0},i,m,min,start,d,j;
    int revPath[MAX_NUM_ROUTERS + 1];

    for(i=0;i< MAX_NUM_ROUTERS + 1;i++)
    {
        dist[i] = INF;
        prev[i] = -1;
    }
    start = source;
    selected[start]=1;
    dist[start] = 0;
    while(selected[target] ==0)
    {
        min = INF;
        m = 0;
        for(i=0; i < no_of_routers; i++)
        {
            d = dist[start] +cost[start][i];
            if(d< dist[i]&&selected[i]==0)
            {
                dist[i] = d;
                prev[i] = start;
            }
            if(min>dist[i] && selected[i]==0)
            {
                min = dist[i];
                m = i;
            }
        }
        start = m;
        selected[start] = 1;
    }
    start = target;
    j = 0;
    while(start != -1)
    {
        revPath[j++] = start;
        start = prev[start];
    }

    for(i = 1; i <= j; i++)
        route[i - 1] = revPath[j - i];

}


