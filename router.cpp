/*
 * router.c
 *
 *  Created on: Nov 7, 2012
 *      Author: shachindra
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "generic.h"

//#define GEN_DEBUG_LOGS

FILE* debugLogs;

FILE* configTopoFile;
FILE* configFile;
FILE* rpFile;

#ifdef TEST_CONNECTIONS
void testConnections()
{
        //test connections
        {
            for(int i = 0; i < no_of_routers; i++)
            {
                //fprintf(debugLogs,"\nloop i = %d, routerId = %d, routerSockFDArr[i] = %d\n",i,routerId,routerSockFDArr[i]);
    
                if((topologyMatrix[routerId][i] != 99) && (topologyMatrix[routerId][i] != 0))
                {
                    char mesg[MAX_BUF_SIZE + 1];
                    char temp[MAX_BUF_SIZE + 1];
    
                    strcpy(mesg,"Message from ");
                    sprintf(temp,"%d",routerId);
                    strcat(mesg,temp);
    
                    fprintf(debugLogs, "\nRouter : %d bytes sent to router %d\n",send(routerSockFDArr[i],mesg,strlen(mesg),0),i);
                    fflush(debugLogs);
                }
            }
        }
    
        for(int i = 0; i < no_of_routers; i++)
        {
            if((topologyMatrix[routerId][i] != 99) && (topologyMatrix[routerId][i] != 0))
            {
                char mesg[MAX_BUF_SIZE + 1];
                int datalen = recv(routerSockFDArr[i], mesg, MAX_BUF_SIZE, 0);
                mesg[datalen] = 0x00;
    
                fprintf(debugLogs,"\nmessage recd from router %d, message : %s\n",i,mesg);
                fflush(debugLogs);
            }
        }
    
        for(int i = 0; i < no_of_routers; i++)
        {
            if(routerSockFDArr[i] != -1)
            {
                close(routerSockFDArr[i]);
                routerSockFDArr[i] = -1;
            }
        }
}
#endif

int getRP(char* fileName, int mGroup)
{
    int rpRouterID = -1;
    char buffer[MAX_BUF_SIZE + 1];
    int arg1,arg2;

    fprintf(debugLogs,"\nmGroup : %d\n",mGroup);
    rpFile = fopen(fileName,"r");
    while(1)
    {
        fprintf(debugLogs,"*");
        fread((void *)buffer,4,1,rpFile);
        buffer[4] = 0x00;
        fprintf(debugLogs,"%s",buffer);

        char temp[2];
        temp[0] = buffer[0];
        temp[1] = 0x00;

        int arg1 = atoi(temp);
        if(arg1 == mGroup)
        {
            temp[0] = buffer[2];
            temp[1] = 0x00;
            arg2 = atoi(temp);
            rpRouterID = arg2;
            break;
        }
        if(feof(rpFile))
        {
            fprintf(debugLogs,"\nRouter : end of file reached\n");
            fflush(debugLogs);
            return -1;
        }
    }
    fclose(rpFile);
    return rpRouterID;
}

int main(int argc, char **argv)
{
#ifdef GEN_DEBUG_LOGS
    char fileName[MAX_BUF_SIZE];

    strcpy(fileName,"debugLogs_");
    strcat(fileName,argv[1]);
    strcat(fileName,".txt");
    debugLogs = fopen(fileName,"w+");
#else
    debugLogs = stdout;
#endif

    if(argc != 5)
    {
        fprintf(debugLogs,"\nusage: router <routerID> <configFile> <config-rp> <config-topo>\n");
        fflush(debugLogs);
        return -1;
    }

    struct sockaddr_in clientRouterAddr;

    int addrlen = sizeof(clientRouterAddr);

    //no of hosts connected to the router
    int no_of_connected_hosts = 0;

    int mGroupsOfRouter[MAX_NUM_MULTICAST_GROUPS];
    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
        mGroupsOfRouter[i] = -1;

    fd_set master;
    fd_set read_fds;
    int fdmax;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    int i = 0,j = 0;

    int myPortNumber;

    //Flag to indicate that SSJOIN has been sent; So, dont send MCAST message back to the RP
    //Change this logic
    int flag = 0;
    int no_of_routers = 0;
    int routerId = -1;

    //Multicast Router Forwarding Table : specifies which routers to forward to
    int MCRFTable[MAX_NUM_MULTICAST_GROUPS][MAX_NUM_ROUTERS];
    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
    {
        for(int j = 0; j < MAX_NUM_ROUTERS; j++)
            MCRFTable[i][j] = -1;
    }

    //Source Specific multicast Router Forwarding Table
    int SSMCRFTable[MAX_NUM_ROUTERS][MAX_NUM_MULTICAST_GROUPS][MAX_NUM_ROUTERS];
    for(int i = 0; i < MAX_NUM_ROUTERS; i++)
    {
        for(int j = 0; j < MAX_NUM_MULTICAST_GROUPS; j++)
        {
            for(int k = 0; k < MAX_NUM_ROUTERS; k++)
                SSMCRFTable[i][j][k] = -1;
        }
    }

    //Multicast Host Forwarding Table : specifies which hosts to forward to
    int MCHFTable[MAX_NUM_MULTICAST_GROUPS][MAX_NUM_HOSTS];
    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
    {
        for(int j = 0; j < MAX_NUM_HOSTS; j++)
            MCHFTable[i][j] = -1;
    }

    //index 0 - router with id 0 and so on.
    int routerSockFDArr[MAX_NUM_ROUTERS];
    for(i = 0; i < MAX_NUM_ROUTERS; i++)
    {
        routerSockFDArr[i] = -1;
    }

    //index 0 - host with id {i0}
    int hostSockFDArr[MAX_NUM_HOSTS];
    for(i = 0; i < MAX_NUM_HOSTS; i++)
    {
        hostSockFDArr[i] = -1;
    }

    //A RP will always belong to the group for which it is the RP
    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
    {
        int rpID = getRP(argv[3],i);
        if(rpID == -1)
            break;
        if(rpID == routerId)
            mGroupsOfRouter[i] = 1;
    }

    //destination, next hop
    int routingTable[MAX_NUM_ROUTERS][2];

    //initialize the matrix
    for(i = 0; i < MAX_NUM_ROUTERS; i++)
    {
        for(j = 0; j < 2; j++)
            routingTable[i][j] = -1;
    }

    int topologyMatrix[MAX_NUM_ROUTERS][MAX_NUM_ROUTERS];

    //initialize the matrix
    for(i = 0; i < MAX_NUM_ROUTERS; i++)
    {
        for(j = 0; j < MAX_NUM_ROUTERS; j++)
            topologyMatrix[i][j] = 0;
    }

    routerId = atoi(argv[1]);

    configTopoFile = fopen(argv[4],"r");

    fscanf(configTopoFile,"%d",&no_of_routers);

    //read OK; tested
    for(i = 0; i < no_of_routers; i++)
    {
        for(j = 0; j < no_of_routers; j++)
        {
            fscanf(configTopoFile,"%d",&topologyMatrix[i][j]);
        }
    }

    //apply dijkstra's algorithm here to find out the single source shortest path;
    //i.e, the shortest path from the router to each of the other routers.

    int shortestRoute[MAX_NUM_ROUTERS + 1];

    //printf("\nsource    next hop\n");
    for(int i = 0; i < no_of_routers; i++)
    {
        for(int j = 0; j < (MAX_NUM_ROUTERS + 1); j++)
            shortestRoute[j] = -1;

        if(routerId == i)
            continue;
        if(routerId <= i)
        {
            dijkstra(topologyMatrix,no_of_routers,routerId,i,shortestRoute);
            routingTable[i][0] = i;
            routingTable[i][1] = shortestRoute[1];
        }
        else
        {
            dijkstra(topologyMatrix,no_of_routers,i,routerId,shortestRoute);
            int it = 0;
            while(shortestRoute[it] != -1)
            {
                it++;
            }
            it--;
            it--;
            routingTable[i][0] = i;
            routingTable[i][1] = shortestRoute[it];
        }
        //printf("\nShortestRoute : %d %d %d %d\n",shortestRoute[0],shortestRoute[1],shortestRoute[2],shortestRoute[3]);
        //printf("\n%d\t%d\n",routerId,shortestRoute[1]);

    }

    //depending on the topology, create connections between the routers.
    //when a router starts, try to connect to other routers to which it is connected.
    //if the connection fails, just keep this router running so that
    //the connection does not fail when the other router tries to connect to this router

    fprintf(debugLogs,"\ntrying to establish connections with other routers\n");
    fflush(debugLogs);

    for(int i = 0; i < no_of_routers; i++)
    {

        fprintf(debugLogs,"\nloop i = %d\n",i);
        fflush(debugLogs);

        if((topologyMatrix[routerId][i] != 99) && (topologyMatrix[routerId][i] != 0))
        {
            fprintf(debugLogs,"\nlink exists; trying to connect\n",i);
            fflush(debugLogs);

            //link exists; establish connection
            struct sockaddr_in      routerAddr;
            struct hostent          *h_router;

            char routerName[MAX_HOSTNAME_LENGTH + 1];
            int portNumber;

            configFile = fopen(argv[2],"r");
            if(!configFile)
            {
                fprintf(debugLogs,"\nError : configFile open failed\n");
                fflush(debugLogs);
                return -1;
            }

            char buffer[MAX_BUF_SIZE + 1];

            for(int j = 0; j <= i; j++)
                fseek(configFile,fscanf(configFile,"%[^\n]s",buffer),SEEK_CUR);

            int rid;
            sscanf(buffer,"%d%s%d",&rid,routerName,&portNumber);

            //fprintf(debugLogs,"\nrid : %d, routerName : %s, portNumber : %d\n",rid, routerName, portNumber);
            //fflush(debugLogs);

            //get the port number of this router
            fseek(configFile,0,SEEK_SET);
            for(int j = 0; j <= no_of_routers; j++)
            {
                fseek(configFile,fscanf(configFile,"%[^\n]s",buffer),SEEK_CUR);
                sscanf(buffer,"%d%s%d",&rid,routerName,&myPortNumber);

                if(rid == routerId)
                {
                    break;
                }
            }


            fclose(configFile);

            if ((h_router = gethostbyname(routerName)) == NULL)
            {
                fprintf(debugLogs,"\nrouter: can't get server address\n");
                fflush(debugLogs);
                return -1;
            }

            bzero(&routerAddr, sizeof(routerAddr));  /* set everything to zero */
            routerAddr.sin_family      = AF_INET;
            routerAddr.sin_port        = htons(portNumber);
            routerAddr.sin_addr = *((struct in_addr *) h_router->h_addr);

            if ((routerSockFDArr[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
            {
                fprintf(debugLogs,"\nrouter: can't open stream socket\n");
                fflush(debugLogs);
                return -1;
            }

            if (connect(routerSockFDArr[i], (struct sockaddr *)&routerAddr, sizeof(struct sockaddr)) < 0)
            {
                fprintf(debugLogs,"\nrouter: can't connect to router\n");
                fflush(debugLogs);
                routerSockFDArr[i] = -1;
            }
            else
            {
                fprintf(debugLogs,"\nConnection successful : (i = %d)\n",i);
                fflush(debugLogs);
            }
        }
    }

    fprintf(debugLogs,"\nListening for connections\n",i);
    fflush(debugLogs);

    struct sockaddr_in      routerAddr;

    int    listeningSockFD = -1;
    if ((listeningSockFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(debugLogs,"\nrouter: can't open stream socket\n");
        fflush(debugLogs);
        return -1;
    }

    bzero(&routerAddr, sizeof(routerAddr));  /* set everything to zero */
    routerAddr.sin_family      = AF_INET;
    routerAddr.sin_port        = htons(myPortNumber);
    routerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    fprintf(debugLogs,"\nrouter: my port number : %d\n",myPortNumber);
    fflush(debugLogs);

    if (bind(listeningSockFD, (struct sockaddr *) &routerAddr, sizeof(routerAddr)) < 0)
    {
        fprintf(debugLogs,"\nrouter: can't bind local address\n");
        fflush(debugLogs);
        return -1;
    }

    listen(listeningSockFD, MAX_PENDING);

    //change this logic to accept connections in any order
    for(int i = 0; i < no_of_routers; i++)
    {
        fprintf(debugLogs,"\nloop i = %d, routerId = %d, routerSockFDArr[i] = %d\n",i,routerId,routerSockFDArr[i]);
        fflush(debugLogs);

        if((topologyMatrix[routerId][i] != 99) && (topologyMatrix[routerId][i] != 0) && (routerSockFDArr[i] == -1))
        {
            fprintf(debugLogs,"\nrouter: waiting for connection from router %d\n",i);
            fflush(debugLogs);

            if ((routerSockFDArr[i] = accept(listeningSockFD, (struct sockaddr *)&clientRouterAddr, (socklen_t *)&addrlen)) < 0)
            {
                fprintf(debugLogs,"\nrouter: can't accept connection\n");
                fflush(debugLogs);
                return -1;
            }
            else
            {
                fprintf(debugLogs,"\nConnection with router i successful : (i = %d)\n",i);
                fflush(debugLogs);
            }
        }
    }

    fprintf(debugLogs,"\nConnections established with all routers in the network according to topology file\n");

    fprintf(debugLogs,"\n*****ROUTING TABLE*****\n");
    fprintf(debugLogs,"\nDest\tNextHop\n");
    fflush(debugLogs);

    for(int i = 0; i < no_of_routers; i++)
    {
        fprintf(debugLogs,"\n%d\t%d\n",i,routingTable[i][1]);
        fflush(debugLogs);
    }

    FD_SET(listeningSockFD, &master);
    fdmax = listeningSockFD;
    
    for(int i = 0; i < no_of_routers; i++)
    {
        if(routerSockFDArr[i] != -1)
        {
            FD_SET(routerSockFDArr[i],&master);
            if(fdmax < routerSockFDArr[i])
                fdmax = routerSockFDArr[i];
        }
    }
    
    for(int i = 0; i < MAX_NUM_HOSTS; i++)
    {
        if(hostSockFDArr[i] != -1)
        {
            FD_SET(hostSockFDArr[i],&master);
            if(fdmax < hostSockFDArr[i])
                fdmax = hostSockFDArr[i];
        }
    }

    while(1)
    {
        read_fds = master;

        fprintf(debugLogs,"\nRouter : WAITING for activity on any port\n");
        fflush(debugLogs);
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            fprintf(debugLogs,"\nRouter : select error\n");
            fflush(debugLogs);
            return -1;
        }

        if (FD_ISSET(listeningSockFD, &read_fds))
        {
            fprintf(debugLogs,"\nRouter : New host connection detected\n");
            fflush(debugLogs);

            int hostFDIndex = (routerId * MAX_NUM_HOSTS_PER_ROUTER) + no_of_connected_hosts++;
            if ((hostSockFDArr[hostFDIndex] = 
                accept(listeningSockFD, (struct sockaddr *)&clientRouterAddr, (socklen_t *)&addrlen)) < 0)
            {
                fprintf(debugLogs,"\nRouter : can't accept connection from host\n");
                fflush(debugLogs);
                return -1;
            }
            else
            {
                fprintf(debugLogs,"\nRouter : Connection with host successful\n");
                fflush(debugLogs);

                //start listening to newly added host's fd
                FD_SET(hostSockFDArr[hostFDIndex],&master);
                if(fdmax < hostSockFDArr[hostFDIndex])
                    fdmax = hostSockFDArr[hostFDIndex];
            }
        }






        //******************************************************
        //******************************************************
        //Data arrival from Router
        //******************************************************
        //******************************************************
        for(int i = 0; i < no_of_routers; i++)
        {
            if(routerSockFDArr[i] != -1)
            {
                if(FD_ISSET(routerSockFDArr[i], &read_fds))
                {
                    char cmd[MAX_BUF_SIZE + 1];
                    char mesg[MAX_BUF_SIZE + 1];

                    fprintf(debugLogs,"\nRouter : New data arrival from router %d\n",i);
                    fflush(debugLogs);

                    int datalen = recv(routerSockFDArr[i], mesg, MAX_BUF_SIZE, 0);
                    mesg[datalen] = 0x00;

                    fprintf(debugLogs,"\nRouter : Data : %s\n",mesg);
                    fflush(debugLogs);

                    sscanf(mesg,"%s",cmd);

                    if(!strcmp(cmd,"JOIN"))
                    {
                        int myID = -1;
                        int rpID = -1;
                        int mGroup = -1;

                        sscanf(mesg,"%s%d%d%d",cmd,&myID,&rpID,&mGroup);

                        fprintf(debugLogs,"\nRouter : Received JOIN\n");
                        fflush(debugLogs);

                        //getchar();

                        //router is not the RP
                        if(rpID != routerId)
                        {
                            //check if the router is already in the MC tree
                            if(mGroupsOfRouter[mGroup] == 1)
                            {
                                fprintf(debugLogs,"\nRouter : I am already present in the MC group. So adding you, doing nothing else\n");
                                MCRFTable[mGroup][myID] = 1;
                            }
                            else
                            {
                                mGroupsOfRouter[mGroup] = 1;
                                MCRFTable[mGroup][myID] = 1;

                                //Update myId and send the JOIN message to the next hop towards the RP
                                myID = routerId;

                                int nextHop = routingTable[rpID][1];
                                int nextHopSockFD = routerSockFDArr[nextHop];

                                fprintf(debugLogs,"\nRouter : Next hop router is %d : JOIN message sent\n",nextHop);
                                fflush(debugLogs);

                                char command[MAX_BUF_SIZE + 1];
                                char buffer[MAX_BUF_SIZE + 1];
                                
                                strcpy(command,"JOIN ");
                                
                                sprintf(buffer,"%d",myID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",rpID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",mGroup);
                                strcat(command,buffer);

                                int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                if(dataLen < 0)
                                {
                                    fprintf(debugLogs,"\nRouter : send error\n");
                                    fflush(debugLogs);
                                    return -1;
                                }
                                else
                                    fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,rpID);
                                    fflush(debugLogs);

                            }
                        }
                        else                        /* If this is the RP*/
                        {
                            mGroupsOfRouter[mGroup] = 1;
                            MCRFTable[mGroup][myID] = 1;
                            fprintf(debugLogs,"\nRouter : I am the RP\n");
                            fflush(debugLogs);
                        }

                        /*fprintf(debugLogs,"\ncase JOIN : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }*/
                    }
                    else if(!strcmp(cmd,"PRUNE"))
                    {
                        fprintf(debugLogs,"\nRouter : PRUNE received from ROUTER\n");
                        fflush(debugLogs);

                        //getchar();
                        
                        int myID = -1;
                        int rpID = -1;
                        int mGroup = -1;

                        sscanf(mesg,"%s%d%d%d",cmd,&myID,&rpID,&mGroup);

                        MCRFTable[mGroup][myID] = -1;

                        for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            SSMCRFTable[myID][mGroup][i] = -1;

                        //Now check if there is any attached host/router which is still in the mCast tree
                        //If not, send PRUNE message to next Hop router towards RP
                        int sendPrune = 1;

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(SSMCRFTable[routerId][mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(MCRFTable[mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(MCRFTable[mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            int rpID = getRP(argv[3],mGroup);
                            if(routerId != rpID)
                            {
                                //send prune message
                                mGroupsOfRouter[mGroup] = -1;

                                char command[MAX_BUF_SIZE + 1];
                                char buffer[MAX_BUF_SIZE + 1];
                                
                                int nextHop = routingTable[rpID][1];
                                int nextHopSockFD = routerSockFDArr[nextHop];
                                
                                strcpy(command,"PRUNE");
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",routerId);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",rpID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",mGroup);
                                strcat(command,buffer);

                                int dataLen =  send(nextHopSockFD,command,strlen(command),0);
                                if(dataLen < 0)
                                {
                                    fprintf(debugLogs,"\nRouter : send error\n");
                                    fflush(debugLogs);
                                    return -1;
                                }
                                else
                                {
                                    fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,rpID);
                                    fflush(debugLogs);
                                }
                            }
                            /*fprintf(debugLogs,"\ncase PRUNE : Printing MCRFTable\n");
                            for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                            {
                                for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                    fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                                fprintf(debugLogs,"\n");
                            }*/
                        }

                    }
                    else if(!strcmp(cmd,"REGISTER"))
                    {
                        int srcID = -1;
                        int rpID = -1;
                        int mGroup = -1;
                        char data[MAX_BUF_SIZE + 1];

                        sscanf(mesg,"%s%d%d%d%s",cmd,&srcID,&rpID,&mGroup,data);

                        fprintf(debugLogs,"\nRouter : Received REGISTER from %d\n",srcID);
                        fflush(debugLogs);

                        //getchar();
                        
                        if(rpID == routerId)
                        {
                            //I am the RP; send SSJOIN to the src router
                            fprintf(debugLogs,"\nRouter : Received REGISTER from %d, sending SSJOINs\n",srcID);
                            fflush(debugLogs);

                            int myID = routerId;

                            char command[MAX_BUF_SIZE + 1];
                            char buffer[MAX_BUF_SIZE + 1];

                            strcpy(command,"SSJOIN");

                            strcat(command," ");
                            sprintf(buffer,"%d",myID);
                            strcat(command,buffer);

                            strcat(command," ");
                            sprintf(buffer,"%d",srcID);
                            strcat(command,buffer);

                            strcat(command," ");
                            sprintf(buffer,"%d",mGroup);
                            strcat(command,buffer);

                            int nextHop = routingTable[srcID][1];
                            int nextHopSockFD = routerSockFDArr[nextHop];

                            int dataLen = send(nextHopSockFD,command,strlen(command),0);

                            if(dataLen < 0)
                            {
                                fprintf(debugLogs,"\nRouter : send error\n");
                                fflush(debugLogs);
                                return -1;
                            }
                            else
                            {
                                fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,srcID);
                                fflush(debugLogs);
                            }


                            sleep(1);

                            //send MCAST message along the multicast tree

                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(MCRFTable[mGroup][i] == 1)
                                {
                                    //send MCAST message to this router
                                    fprintf(debugLogs,"\nRouter : MCAST message sent to router %d\n",i);
                                    fflush(debugLogs);
                                    //MCAST <myID> <srcID> <mgroup> <data>

                                    strcpy(command,"MCAST");
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",myID);
                                    strcat(command,buffer);
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",srcID);
                                    strcat(command,buffer);
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",mGroup);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    strcat(command,data);

                                    nextHop = routingTable[i][1];
                                    nextHopSockFD = routerSockFDArr[nextHop];

                                    int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                    if(dataLen < 0)
                                    {
                                        fprintf(debugLogs,"\nRouter : send error\n");
                                        fflush(debugLogs);
                                        return -1;
                                    }
                                    else
                                    {
                                        fprintf(debugLogs,"\nRouter : sent %s to router %d, nextHop : %d, nextHopSockFD %d, dataLen = %d\n",command,i,nextHop,nextHopSockFD,dataLen);
                                        fflush(debugLogs);
                                    }

                                }
                            }

                            for(int i = 0; i < MAX_NUM_HOSTS; i++)
                            {
                                if(MCHFTable[mGroup][i] == 1)
                                {
                                    //send MCAST message to this host
                                    fprintf(debugLogs,"\nRouter : MCAST message sent to host %d\n",i);
                                    fflush(debugLogs);

                                    //MCAST <myID> <srcID> <mgroup> <data>
                                    
                                    strcpy(command,"MCAST");
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",myID);
                                    strcat(command,buffer);
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",srcID);
                                    strcat(command,buffer);
                                    
                                    strcat(command," ");
                                    sprintf(buffer,"%d",mGroup);
                                    strcat(command,buffer);
                                    
                                    strcat(command," ");
                                    strcat(command,data);

                                    nextHopSockFD = hostSockFDArr[i];

                                    int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                    if(dataLen < 0)
                                    {
                                        fprintf(debugLogs,"\nRouter : send error\n");
                                        fflush(debugLogs);
                                        return -1;
                                    }
                                    else
                                    {
                                        fprintf(debugLogs,"\nRouter : sent %s to host %d\n",command,i);
                                        fflush(debugLogs);
                                    }

                                }
                            }
                        }
                        else
                        {
                            //I am not the RP; Just forwarding the REGISTER to the next hop router towards the RP
                            int nextHop = routingTable[rpID][1];
                            int nextHopSockFD = routerSockFDArr[nextHop];

                            fprintf(debugLogs,"Router : rpId : %d, nextHop : %d",rpID,nextHop);
                            fflush(debugLogs);

                            int dataLen = send(nextHopSockFD,mesg,strlen(mesg),0);

                            if(dataLen < 0)
                            {
                                fprintf(debugLogs,"\nRouter : send error\n");
                                fflush(debugLogs);
                                return -1;
                            }
                            else
                            {
                                fprintf(debugLogs,"\nRouter : sent %s to router %d\n",mesg,rpID);
                                fflush(debugLogs);
                            }
                        }
                        /*fprintf(debugLogs,"\ncase REGISTER : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }*/
                    }
                    else if(!strcmp(cmd,"SSJOIN"))
                    {
                        int myID = -1;
                        int srcID = -1;
                        int mGroup = -1;

                        sscanf(mesg,"%s%d%d%d",cmd,&myID,&srcID,&mGroup);

                        fprintf(debugLogs,"\nRouter : SSJOIN received\n");
                        fflush(debugLogs);

                        //getchar();
                        
                        SSMCRFTable[srcID][mGroup][myID] = 1;

                        flag = 1;
                        if(srcID != routerId)
                        {
                            fprintf(debugLogs,"\nRouter : I did not send a REGISTER; so forwarding to next hop toward %d\n",srcID);
                            fflush(debugLogs);

                            myID = routerId;

                            char command[MAX_BUF_SIZE + 1];
                            char buffer[MAX_BUF_SIZE + 1];

                            strcpy(command,"SSJOIN");
                            
                            strcat(command," ");
                            sprintf(buffer,"%d",myID);
                            strcat(command,buffer);
                            
                            strcat(command," ");
                            sprintf(buffer,"%d",srcID);
                            strcat(command,buffer);
                            
                            strcat(command," ");
                            sprintf(buffer,"%d",mGroup);
                            strcat(command,buffer);
                            
                            int nextHop = routingTable[srcID][1];
                            int nextHopSockFD = routerSockFDArr[nextHop];
                            
                            int dataLen = send(nextHopSockFD,command,strlen(command),0);
                            
                            if(dataLen < 0)
                            {
                                fprintf(debugLogs,"\nRouter : send error\n");
                                fflush(debugLogs);
                                return -1;
                            }
                            else
                            {
                                fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,srcID);
                                fflush(debugLogs);
                            }

                        }
                        else
                        {
                            fprintf(debugLogs,"\nRouter : I had generated REGISTER.\n",srcID);
                            fflush(debugLogs);
                        }
                        /*fprintf(debugLogs,"\ncase SSJOIN : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }*/
                    }
                    else if(!strcmp(cmd,"MCAST"))
                    {
                        int myID = -1;
                        int srcID = -1;
                        int mGroup = -1;
                        char data[MAX_BUF_SIZE + 1];

                        sscanf(mesg,"%s%d%d%d%s",cmd,&myID,&srcID,&mGroup,data);

                        fprintf(debugLogs,"\nRouter : Received MCAST from %d; data originally sent from %d\n",myID,srcID);
                        fflush(debugLogs);

                        //getchar();
                        
                        for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                         {
                            fprintf(debugLogs,"\nRouter : mGroup : %d, myID : %d, i = %d,MCRFTable[mGroup][i] = %d\n",mGroup,myID,i,MCRFTable[mGroup][i]);
                             if(MCRFTable[mGroup][i] == 1)
                             {
                                int rpID = getRP(argv[3],mGroup);

                                if(/*(rpID == routerId)||*/(i != myID))
                                {
                                     //send MCAST message to this router
                                     fprintf(debugLogs,"\nRouter : MCAST message sent to router %d\n",i);
                                     fflush(debugLogs);
                                     //MCAST <myID> <srcID> <mgroup> <data>

                                     char command[MAX_BUF_SIZE + 1];
                                     char buffer[MAX_BUF_SIZE + 1];
                                     myID = routerId;

                                     strcpy(command,"MCAST");
                                     
                                     strcat(command," ");
                                     sprintf(buffer,"%d",myID);
                                     strcat(command,buffer);
                                     
                                     strcat(command," ");
                                     sprintf(buffer,"%d",srcID);
                                     strcat(command,buffer);
                                     
                                     strcat(command," ");
                                     sprintf(buffer,"%d",mGroup);
                                     strcat(command,buffer);
                            
                                     strcat(command," ");
                                     strcat(command,data);
                            
                                     int nextHop = routingTable[i][1];
                                     int nextHopSockFD = routerSockFDArr[nextHop];
                            
                                     int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                     if(dataLen < 0)
                                     {
                                         fprintf(debugLogs,"\nRouter : send error\n");
                                         fflush(debugLogs);
                                         return -1;
                                     }
                                     else
                                     {
                                         fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,i);
                                         fflush(debugLogs);
                                     }
                                 }
                             }
                         }

                         for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                         {
                             if(SSMCRFTable[srcID][mGroup][i] != -1)
                             {
                                if(flag)
                                {
                                    flag = 0;
                                    continue;
                                }
                                //fprintf(debugLogs,"\n*****BUGPOINT*****\n");
                                 //send MCAST message
                                 int myID = routerId;
                         
                                 char command[MAX_BUF_SIZE + 1];
                                 char buffer[MAX_BUF_SIZE + 1];
                         
                                 strcpy(command,"MCAST ");
                         
                                 sprintf(buffer,"%d",myID);
                                 strcat(command,buffer);
                         
                                 strcat(command," ");
                                 sprintf(buffer,"%d",srcID);
                                 strcat(command,buffer);
                         
                                 strcat(command," ");
                                 sprintf(buffer,"%d",mGroup);
                                 strcat(command,buffer);
                         
                                 strcat(command," ");
                                 strcat(command,data);
                         
                                 int nextHop = routingTable[i][1];
                                 int nextHopSockFD = routerSockFDArr[nextHop];
                         
                                 fprintf(debugLogs,"\nNextHop : %d, nextHopSockFD : %d\n",nextHop,nextHopSockFD);
                                 int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                 if(dataLen < 0)
                                 {
                                     fprintf(debugLogs,"\nRouter : send error\n");
                                     fflush(debugLogs);
                                     return -1;
                                 }
                                 else
                                 {
                                     fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,i);
                                     fflush(debugLogs);
                                 }
                             }
                         }

                         for(int i = 0; i < MAX_NUM_HOSTS; i++)
                        {
                            if(MCHFTable[mGroup][i] == 1)
                            {
                                char command[MAX_BUF_SIZE + 1];
                                char buffer[MAX_BUF_SIZE + 1];

                                //send MCAST message to this host
                                fprintf(debugLogs,"\nRouter : MCAST message sent to host %d\n",i);
                                fflush(debugLogs);

                                //MCAST <myID> <srcID> <mgroup> <data>
                                
                                strcpy(command,"MCAST");
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",myID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",srcID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",mGroup);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                strcat(command,data);

                                int nextHopSockFD = hostSockFDArr[i];

                                int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                if(dataLen < 0)
                                {
                                    fprintf(debugLogs,"\nRouter : send error\n");
                                    fflush(debugLogs);
                                    return -1;
                                }
                                else
                                {
                                    fprintf(debugLogs,"\nRouter : sent %s to host %d\n",command,i);
                                    fflush(debugLogs);
                                }

                            }
                        }
                        /*fprintf(debugLogs,"\ncase MCAST : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }*/
                    }
                    else
                    {
                        //Invalid cmd received
                        fprintf(debugLogs,"\nRouter : Invalid command received\n");
                        fflush(debugLogs);
                        return -1;
                    }
                }
            }
        }





        //************************************************************
        //************************************************************
        //Data arrival from Host
        //************************************************************
        //************************************************************
        for(int i = 0; i < no_of_connected_hosts; i++)
        {
            if(hostSockFDArr[(routerId * MAX_NUM_HOSTS_PER_ROUTER) + i] != -1)
            {
                if(FD_ISSET(hostSockFDArr[(routerId * MAX_NUM_HOSTS_PER_ROUTER) + i], &read_fds))
                {
                    char cmd[MAX_BUF_SIZE + 1];
                    char mesg[MAX_BUF_SIZE + 1];

                    fprintf(debugLogs,"\nRouter : New data arrival from host %d\n",(routerId * MAX_NUM_HOSTS_PER_ROUTER) + i);
                    fflush(debugLogs);

                    int datalen = recv(hostSockFDArr[(routerId * MAX_NUM_HOSTS_PER_ROUTER) + i], mesg, MAX_BUF_SIZE, 0);
                    mesg[datalen] = 0x00;

                    fprintf(debugLogs,"\nRouter : Data : %s\n",mesg);
                    fflush(debugLogs);

                    sleep(1);

                    sscanf(mesg,"%s",cmd);

                    if(!strcmp(cmd,"REPORT"))
                    {
                        fprintf(debugLogs,"\nRouter : REPORT received from HOST\n");
                        fflush(debugLogs);

                        //getchar();
                        
                        int myID = -1;
                        int mGroup = -1;
                        int rpRouterID = -1;
 
                        sscanf(mesg,"%s%d%d",cmd,&myID,&mGroup);

                        //adding the host to the list of members of MC group
                        MCHFTable[mGroup][myID] = 1;

                        if(mGroupsOfRouter[mGroup] != 1)
                        {
                            //send join message
                            mGroupsOfRouter[mGroup] = 1; 

                            rpRouterID = getRP(argv[3],mGroup);
                            if(rpRouterID == routerId)
                            {
                                //I am the RP; update my forwarding state for this multicast group
                                fprintf(debugLogs,"\nRouter : I am the RP for this MG\n");
                                fflush(debugLogs);
                            }
                            else
                            {
                                //I am not the RP; progpagate the JOIN message towards the RP
                                int nextHop = routingTable[rpRouterID][1];
                                int nextHopSockFD = routerSockFDArr[nextHop];
                        
                                fprintf(debugLogs,"\nRouter : Next hop router is %d : JOIN message sent\n",nextHop);
                                fflush(debugLogs);
                        
                                char command[MAX_BUF_SIZE + 1];
                                char buffer[MAX_BUF_SIZE + 1];
                        
                                strcpy(command,"JOIN ");
                        
                                //sprintf(buffer,"%d",myID);
                                sprintf(buffer,"%d",routerId);
                                strcat(command,buffer);
                        
                                strcat(command," ");
                                sprintf(buffer,"%d",rpRouterID);
                                strcat(command,buffer);
                        
                                strcat(command," ");
                                sprintf(buffer,"%d",mGroup);
                                strcat(command,buffer);
                        
                                fprintf(debugLogs, "\nRouter : %d bytes sent to router %d\n",
                                    send(nextHopSockFD,command,strlen(command),0),rpRouterID);
                                fflush(debugLogs);
                            }
                        }
                        else
                        {
                            fprintf(debugLogs,"\nRouter : I'm already present in MG. Adding host to the MG\n");
                            fflush(debugLogs);
                        }
/*
                        fprintf(debugLogs,"\ncase REPORT : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }
*/
                    }
                    else if(!strcmp(cmd,"SEND"))
                    {
                        fprintf(debugLogs,"\nRouter : SEND received from HOST\n");
                        fflush(debugLogs);

                        //getchar();
                        
                        int myID = -1;
                        int mGroup = -1;
                        char data[MAX_BUF_SIZE + 1];

                        sscanf(mesg,"%s%d%d%s",cmd,&myID,&mGroup,data);

                        //check if this router is the RP for the group;
                        //if yes, just multicast the message to all the routers and hosts in the group
                        {
                            int rpID = getRP(argv[3],mGroup);
                            if(rpID == routerId)
                            {
                                for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                                {
                                    if(MCRFTable[mGroup][i] != -1)
                                    {
                                        //send MCAST message
                                        int myID = routerId;
                                
                                        int srcID = routerId;
                                
                                        char command[MAX_BUF_SIZE + 1];
                                        char buffer[MAX_BUF_SIZE + 1];
                                
                                        strcpy(command,"MCAST ");
                                
                                        sprintf(buffer,"%d",myID);
                                        strcat(command,buffer);
                                
                                        strcat(command," ");
                                        sprintf(buffer,"%d",srcID);
                                        strcat(command,buffer);
                                
                                        strcat(command," ");
                                        sprintf(buffer,"%d",mGroup);
                                        strcat(command,buffer);
                                
                                        strcat(command," ");
                                        strcat(command,data);
                                
                                        int nextHop = routingTable[i][1];
                                        int nextHopSockFD = routerSockFDArr[nextHop];
                                
                                        fprintf(debugLogs,"\nNextHop : %d, nextHopSockFD : %d\n",nextHop,nextHopSockFD);
                                        int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                        if(dataLen < 0)
                                        {
                                            fprintf(debugLogs,"\nRouter : send error\n");
                                            fflush(debugLogs);
                                            return -1;
                                        }
                                        else
                                        {
                                            fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,i);
                                            fflush(debugLogs);
                                        }
                                    }
                                }

                                for(int i = 0; i < MAX_NUM_HOSTS; i++)
                                {
                                    if((MCHFTable[mGroup][i] != -1)/* && (i != myID)*/)
                                    {
                                        //send MCAST message
                                        int myID = routerId;
                                
                                        int srcID = routerId;
                                
                                        char command[MAX_BUF_SIZE + 1];
                                        char buffer[MAX_BUF_SIZE + 1];
                                
                                        strcpy(command,"MCAST ");
                                
                                        sprintf(buffer,"%d",myID);
                                        strcat(command,buffer);
                                
                                        strcat(command," ");
                                        sprintf(buffer,"%d",srcID);
                                        strcat(command,buffer);
                                
                                        strcat(command," ");
                                        sprintf(buffer,"%d",mGroup);
                                        strcat(command,buffer);

                                        strcat(command," ");
                                        strcat(command,data);
                                
                                        int nextHopSockFD = hostSockFDArr[i];
                                
                                        int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                        if(dataLen < 0)
                                        {
                                            fprintf(debugLogs,"\nRouter : send error\n");
                                            fflush(debugLogs);
                                            return -1;
                                        }
                                        else
                                        {
                                            fprintf(debugLogs,"\nRouter : sent %s to host %d\n",command,i);
                                            fflush(debugLogs);
                                        }
                                    }
                                }

                                continue;
                            }
                        }   
                        //check if router has a source specific forwarding state entry indicating itself as source
                        int SSFSpresent = 0;

                        for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                        {
                            if(SSMCRFTable[routerId][mGroup][i] != -1)
                            {
                                SSFSpresent = 1;
                                break;
                            }
                        }

                        if(SSFSpresent)
                        {
                            fprintf(debugLogs,"\nRouter : SSFS is present for (routerId %d,mGroup %d)\n",routerId,mGroup);
                            fflush(debugLogs);
                        }
                        else
                        {
                            fprintf(debugLogs,"\nRouter : SSFS is not present for (routerId %d,mGroup %d)\n",routerId,mGroup);
                            fflush(debugLogs);
                        }

                        //check if the router is already in the MC tree
                        if((mGroupsOfRouter[mGroup] == 1)&& (SSFSpresent))
                        {
                            //add the host to the multicast forwarding table
                            //MCHFTable[mGroup][myID] = 1;//*********************************************************

                            //just send the MCAST message
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(SSMCRFTable[routerId][mGroup][i] != -1)
                                {
                                    //send MCAST message
                                    int myID = routerId;

                                    int srcID = routerId;

                                    char command[MAX_BUF_SIZE + 1];
                                    char buffer[MAX_BUF_SIZE + 1];

                                    strcpy(command,"MCAST ");

                                    sprintf(buffer,"%d",myID);
                                    strcat(command,buffer);
                            
                                    strcat(command," ");
                                    sprintf(buffer,"%d",srcID);
                                    strcat(command,buffer);
                            
                                    strcat(command," ");
                                    sprintf(buffer,"%d",mGroup);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    strcat(command,data);

                                    int nextHop = routingTable[i][1];
                                    int nextHopSockFD = routerSockFDArr[nextHop];

                                    fprintf(debugLogs,"\nNextHop : %d, nextHopSockFD : %d\n",nextHop,nextHopSockFD);
                                    int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                    if(dataLen < 0)
                                    {
                                        fprintf(debugLogs,"\nRouter : send error\n");
                                        fflush(debugLogs);
                                        return -1;
                                    }
                                    else
                                    {
                                        fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,i);
                                        fflush(debugLogs);
                                    }
                                }
                            }

                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(MCRFTable[mGroup][i] != -1)
                                {
                                    //send MCAST message
                                    int myID = routerId;

                                    int srcID = routerId;

                                    char command[MAX_BUF_SIZE + 1];
                                    char buffer[MAX_BUF_SIZE + 1];

                                    strcpy(command,"MCAST ");

                                    sprintf(buffer,"%d",myID);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    sprintf(buffer,"%d",srcID);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    sprintf(buffer,"%d",mGroup);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    strcat(command,data);

                                    int nextHop = routingTable[i][1];
                                    int nextHopSockFD = routerSockFDArr[nextHop];

                                    int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                    if(dataLen < 0)
                                    {
                                        fprintf(debugLogs,"\nRouter : send error\n");
                                        fflush(debugLogs);
                                        return -1;
                                    }
                                    else
                                    {
                                        fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,i);
                                        fflush(debugLogs);
                                    }
                                }

                            }

                            for(int i = 0; i < MAX_NUM_HOSTS; i++)
                            {
                                if((MCHFTable[mGroup][i] != -1)/* && (i != myID)*/)
                                {
                                    //send MCAST message
                                    int myID = routerId;

                                    int srcID = routerId;

                                    char command[MAX_BUF_SIZE + 1];
                                    char buffer[MAX_BUF_SIZE + 1];

                                    strcpy(command,"MCAST ");

                                    sprintf(buffer,"%d",myID);
                                    strcat(command,buffer);
                            
                                    strcat(command," ");
                                    sprintf(buffer,"%d",srcID);
                                    strcat(command,buffer);
                            
                                    strcat(command," ");
                                    sprintf(buffer,"%d",mGroup);
                                    strcat(command,buffer);

                                    strcat(command," ");
                                    strcat(command,data);

                                    int nextHopSockFD = hostSockFDArr[i];

                                    int dataLen = send(nextHopSockFD,command,strlen(command),0);
                                    if(dataLen < 0)
                                    {
                                        fprintf(debugLogs,"\nRouter : send error\n");
                                        fflush(debugLogs);
                                        return -1;
                                    }
                                    else
                                    {
                                        fprintf(debugLogs,"\nRouter : sent %s to host %d\n",command,i);
                                        fflush(debugLogs);
                                    }
                                }
                            }
                        }
                        else
                        {
                            fprintf(debugLogs,"\nRouter : forwarding state entry doesn't exist; sending REGISTER towards RP\n");
                            fflush(debugLogs);

                            mGroupsOfRouter[mGroup] = 1;
                            MCHFTable[mGroup][myID] = 1;

                            char command[MAX_BUF_SIZE + 1];
                            char buffer[MAX_BUF_SIZE + 1];

                            int rpRouterID = getRP(argv[3],mGroup);

                            int nextHop = routingTable[rpRouterID][1];
                            int nextHopSockFD = routerSockFDArr[nextHop];

                            strcpy(command,"REGISTER");

                            strcat(command," ");
                            sprintf(buffer,"%d",routerId);
                            strcat(command,buffer);

                            strcat(command," ");
                            sprintf(buffer,"%d",rpRouterID);
                            strcat(command,buffer);

                            strcat(command," ");
                            sprintf(buffer,"%d",mGroup);
                            strcat(command,buffer);

                            strcat(command," ");
                            strcat(command,data);

                            int dataLen =  send(nextHopSockFD,command,strlen(command),0);
                            if(dataLen < 0)
                            {
                                fprintf(debugLogs,"\nRouter : send error\n");
                                fflush(debugLogs);
                                return -1;
                            }
                            else
                            {
                                fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,rpRouterID);
                                fflush(debugLogs);
                            }
                        }
/*
                        fprintf(debugLogs,"\ncase SEND : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }*/
                    }
                    else if(!strcmp(cmd,"LEAVE"))
                    {
                        fprintf(debugLogs,"\nRouter : LEAVE received from HOST\n");
                        fflush(debugLogs);

                        //getchar();
                        
                        int myID = -1;
                        int mGroup = -1;

                        sscanf(mesg,"%s%d%d",cmd,&myID,&mGroup);

                        MCHFTable[mGroup][myID] = -1;

                        //Now check if there is any attached host/router which is still in the mCast tree
                        //If not, send PRUNE message to next Hop router towards RP
                        int sendPrune = 1;

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(SSMCRFTable[routerId][mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_ROUTERS; i++)
                            {
                                if(MCRFTable[mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            for(int i = 0; i < MAX_NUM_HOSTS; i++)
                            {
                                if(MCHFTable[mGroup][i] != -1)
                                {
                                    sendPrune = 0;
                                    break;
                                }
                            }
                        }

                        if(sendPrune)
                        {
                            int rpID = getRP(argv[3],mGroup);
                            if(routerId != rpID)
                            {
                                //send prune message
                                mGroupsOfRouter[mGroup] = -1;

                                char command[MAX_BUF_SIZE + 1];
                                char buffer[MAX_BUF_SIZE + 1];
                                
                                int nextHop = routingTable[rpID][1];
                                int nextHopSockFD = routerSockFDArr[nextHop];
                                
                                strcpy(command,"PRUNE");
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",routerId);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",rpID);
                                strcat(command,buffer);
                                
                                strcat(command," ");
                                sprintf(buffer,"%d",mGroup);
                                strcat(command,buffer);

                                int dataLen =  send(nextHopSockFD,command,strlen(command),0);
                                if(dataLen < 0)
                                {
                                    fprintf(debugLogs,"\nRouter : send error\n");
                                    fflush(debugLogs);
                                    return -1;
                                }
                                else
                                {
                                    fprintf(debugLogs,"\nRouter : sent %s to router %d\n",command,rpID);
                                    fflush(debugLogs);
                                }
                            }
                        }
/*
                        fprintf(debugLogs,"\ncase LEAVE : Printing MCRFTable\n");
                        for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                        {
                            for(int j = 0; j < MAX_NUM_ROUTERS; j++)
                                fprintf(debugLogs,"%d\t",MCRFTable[i][j]);
                            fprintf(debugLogs,"\n");
                        }
*/
                    }
                    else
                    {
                        fprintf(debugLogs,"\nRouter : Invalid cmd received\n",mesg);
                        fflush(debugLogs);
                        //return -1;
                    }
                }
            }
        }

    }
    return 0;
}

