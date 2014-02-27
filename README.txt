Steps to execute the program : 

1 - Run the makefile by entering the command 'make'.
2 - Start all the routers in ascending order of the router IDs.
3 - Connect the hosts as appropriate.
4 - Join the hosts to the multicast groups using the command 'JOIN <mGroup>'.
5 - To remove the hosts from the multicast group use the 'LEAVE <mGroup>' command.
6 - If the only host connected to the router is removed from the mGroup, check the PRUNE message sent by the router to the RP.
7 - Send the messages any of the multicast group using the command 'SEND <filename> <mGroup>'.
8 - After sending the messages, you can check the SSJOIN or MCAST messages sent by the router in the 'debuglogs' file.