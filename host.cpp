/*
 * host.c
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

FILE* debugLogs;

int main(int argc, char **argv)
{
#ifdef GEN_DEBUG_LOGS
    debugLogs = fopen("debugLogs.txt","w+");
#else
    debugLogs = stdout;
#endif

    int hostSockfd = 0;
    struct sockaddr_in routerAddr;
    struct hostent *h_router;

    fd_set master;
    fd_set read_fds;
    int fdmax;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    int dataLen = 0;

    if(argc < 4)
    {
        fprintf(debugLogs,"\nusage: host <hostID> <configFile> <myRouterID> <mGroup(optional)>\n");
        return -1;
    }

    char hostId[MAX_LENGTH_HOSTID + 1];
    char configFile[MAX_LENGTH_FILENAME + 1];
    char myRouterID[MAX_LENGTH_ROUTERID + 1];
    char mGroup[MAX_LENGTH_GROUP + 1];
    char command[MAX_BUF_SIZE + 1];
    char tempBuf[MAX_BUF_SIZE + 1];

    int mGroupList[MAX_NUM_MULTICAST_GROUPS];
    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
        mGroupList[i] = -1;

    strcpy(hostId,argv[1]);
    strcpy(configFile,argv[2]);
    strcpy(myRouterID,argv[3]);

    if(argc == 5)
        strcpy(mGroup,argv[4]);
    else
        strcpy(mGroup,"");

    FILE* configFileFP = fopen(configFile,"r");

    char configFileLine[MAX_CONFIGFILE_TUPLE_LENGTH + 1];

    int done = 0;

    while(!done)
    {
        if(fgets(configFileLine,MAX_CONFIGFILE_TUPLE_LENGTH,configFileFP) == NULL)
        {
            fprintf(debugLogs,"\nHOST : ERROR : Router ID not found in file\n");
            return -1;
        }

        char routerID[MAX_LENGTH_ROUTERID + 1];
        sscanf(configFileLine,"%[^ ]s",routerID);

        if(!strcmp(routerID,myRouterID))
            done = 1;
    }

    char hostName[MAX_HOSTNAME_LENGTH + 1];
    char portNumber[MAX_PORTNUM_LENGTH + 1];

    {
        int tempIndex = 0;
        int i = 0;

        //omit the first word i.e router ID
        while(configFileLine[tempIndex++] != ' ');

        while(configFileLine[tempIndex] != ' ')
            hostName[i++] = configFileLine[tempIndex++];

        hostName[i] = 0x00;
        i = 0;
        tempIndex++;

        while(configFileLine[tempIndex] != 0x0A)
            portNumber[i++] = configFileLine[tempIndex++];

        portNumber[i] = 0x00;
        i = 0;
    }

    if ((h_router = gethostbyname(hostName)) == NULL)
    {
        fprintf(debugLogs,"\nhost: can't get router address\n");
        return -1;
    }

    bzero(&routerAddr, sizeof(routerAddr));  /* set everything to zero */
    routerAddr.sin_family      = AF_INET;
    routerAddr.sin_port        = htons(atoi(portNumber));
    routerAddr.sin_addr        = *((struct in_addr *) h_router->h_addr);

    if ((hostSockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(debugLogs,"client: can't open stream socket");
        return -1;
    }

    if (connect(hostSockfd, (struct sockaddr *)&routerAddr, sizeof(struct sockaddr)) < 0)
    {
        fprintf(debugLogs,"host: can't connect to my router");
        return -1;
    }
    else
        fprintf(debugLogs,"host: connection established with my router");

    if(argc == 5)
    {
        //join a multicast group
        //send REPORT message

        strcpy(command,"REPORT ");
        strcat(command,argv[1]);
        strcat(command," ");
        strcat(command,mGroup);

        dataLen = send(hostSockfd, command, strlen(command), 0);
        if(dataLen == -1)
        {
            fprintf(debugLogs,"\nHost : send error\n");
            return -1;
        }
        else
        {
            fprintf(debugLogs,"\nHost : Sent %s to my router\n",command);
            mGroupList[atoi(mGroup)] = 1;
        }
    }

    FD_SET(fileno(stdin), &master);
    FD_SET(hostSockfd, &master);
    fdmax = hostSockfd;

    //give options to the user
    while(1)
    {
        printf("\n1 - SEND <filename> <mGroup>");
        printf("\n2 - JOIN <mGroup>");
        printf("\n3 - LEAVE <mGroup>");
        printf("\n4 - LIST\n");

        read_fds = master;

        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            fprintf(debugLogs,"host: select error");
            return -1;
        }

        if (FD_ISSET(fileno(stdin), &read_fds))
        {
            fprintf(debugLogs,"\nHost : User input detected\n");
            scanf("%[^\n]s",command);
            getchar();

            fprintf(debugLogs,"\nuser input : %s\n",command);

            char cmd[MAX_BUF_SIZE + 1];
            char arg1[MAX_BUF_SIZE + 1];
            char arg2[MAX_BUF_SIZE + 1];
            char tempBuf[MAX_BUF_SIZE + 1];

            sscanf(command,"%s",cmd);

            fprintf(debugLogs,"\nuser input : %s\n",cmd);
            if(!strcmp(cmd,"SEND"))
            {
                sscanf(command,"%s%s%s",cmd,arg1,arg2);

                FILE* fp = fopen(arg1,"r");
                fseek(fp,0,SEEK_END);
                int fileSize = ftell(fp);

                fseek(fp,0,SEEK_SET);
                fread (tempBuf, (fileSize - 1),1,fp);
                tempBuf[fileSize - 1] = 0x00;

                fclose(fp);

                strcpy(command,"SEND ");
                strcat(command,hostId);
                strcat(command," ");
                strcat(command,arg2);
                strcat(command," ");
                strcat(command,tempBuf);

                dataLen = send(hostSockfd, command, strlen(command), 0);
                if(dataLen < 0)
                {
                    fprintf(debugLogs,"\nHost : send error\n");
                    return -1;
                }
                else
                    fprintf(debugLogs,"\nHost : sent %s to my router\n",command);
            }
            else if(!strcmp(cmd,"JOIN"))
            {
                sscanf(command,"%s%s",cmd,arg1);
                strcpy(command,"REPORT ");
                strcat(command,argv[1]);
                strcat(command," ");
                strcat(command,arg1);

                dataLen = send(hostSockfd, command, strlen(command), 0);
                if(dataLen == -1)
                {
                    fprintf(debugLogs,"\nHost : send error\n");
                    return -1;
                }
                else
                {
                    fprintf(debugLogs,"\nHost : sent %s to my router\n",command);
                    mGroupList[atoi(arg1)] = 1;
                }

            }
            else if(!strcmp(cmd,"LEAVE"))
            {
                sscanf(command,"%s%s",cmd,arg1);
                if(mGroupList[atoi(arg1)] != -1)
                {
                    fprintf(debugLogs,"\nHost : This host is in mGroup %d, leaving the group\n",atoi(arg1));

                    //Send LEAVE to router
                    strcpy(command,"LEAVE ");
                    strcat(command,argv[1]);
                    strcat(command," ");
                    strcat(command,arg1);

                    dataLen = send(hostSockfd, command, strlen(command), 0);
                    if(dataLen == -1)
                    {
                        fprintf(debugLogs,"\nHost : send error\n");
                        return -1;
                    }
                    else
                    {
                        fprintf(debugLogs,"\nHost : sent %s to my router\n",command);
                        mGroupList[atoi(arg1)] = -1;
                    }
                }
            }
            else if(!strcmp(cmd,"LIST"))
            {
                int isPresent = 0;
                for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                {
                    if(mGroupList[i] != -1)
                    {
                        isPresent = 1;
                        break;
                    }
                }

                if(isPresent)
                {
                    fprintf(debugLogs,"\nHost : This host is in the following mGroups : ");
                    for(int i = 0; i < MAX_NUM_MULTICAST_GROUPS; i++)
                    {
                        if(mGroupList[i] != -1)
                            fprintf(debugLogs,"\t%d",i);
                    }
                    fprintf(debugLogs,"\n");
                }
                else
                    fprintf(debugLogs,"\nHost : This host is not in any mGroup\n");

            }
        }
        if(FD_ISSET(hostSockfd, &read_fds))
        {
            fprintf(debugLogs,"\nHost : Multicast message received\n");

            int myID = -1;
            int srcID = -1;
            int mGroup = -1;
            char data[MAX_BUF_SIZE + 1];
            char cmd[MAX_BUF_SIZE + 1];

            dataLen = recv(hostSockfd, data, MAX_BUF_SIZE, 0);
            if(dataLen == -1)
            {
                fprintf(debugLogs,"\nHost : recv error\n");
                return -1;
            }
            data[dataLen] = 0x00;

            sscanf(data,"%s%d%d%d%s",cmd,&myID,&srcID,&mGroup,data);
            
            fprintf(debugLogs,"\nHost : Received MCAST from %d; data originally sent from %d, mGroup %d\n",myID,srcID,mGroup);
            
            fprintf(debugLogs,"\nHost : MULTICAST message : %s\n",data);

        }
    }
    return 0;
}



