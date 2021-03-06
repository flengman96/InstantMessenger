/*  This File is respsonsible for the client to connect to the server
/   It reads in input from the user and sends it to the server
/ 
/   There are still a few bugs.... When a private connection is initiated with
/   another user and one user logs out, sometimes the other client crashes.
/   This is most likely a memory leak, or a Socket ERROR
/   
/   ZID: 3470429
/   Name: Stephen Comino
/*/


#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <threads.h>
#include <stdlib.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include "clientP2P.h"

// Global variables to hold information
p2pClient client;   // This holds p2p client information
client_info connectionInfo; // This is used to hold the p2p client connect info
p2pServer server_info;  // This holds the server info
int isOn;   // This is used to flag a loop. to break it
p2p_List p2p_ServerClients; // List to hold servers
p2p_List p2p_Clients; // List to hold clients

// Address of the server for p2p
struct sockaddr_in p2p_address;

// This is the main function of the program. It is used to initialise the global
// Structs used throughout the program. It is also used to Start TWO threads
//  
//  1. p2pThread - handles both server/Client p2p interaction
//  2. ClientThread - handles the connection with the server
//
int main(int argc, char **argv) {
    // Create initialisers for these structs
    connectionInfo = malloc (sizeof (struct clientThread));
    server_info = malloc (sizeof (struct p2pServerThread));
    client = malloc (sizeof (struct p2pClientThread));
    connectionInfo->server_name = malloc (sizeof(char) * strlen(argv[1]) + 1);
    //prepare server credentials
    connectionInfo->server_name = argv[1];
    connectionInfo->server_port = atoi(argv[2]);
    //change this port No if required
    server_info->isActive = 0;
    client->isActive = 0;
    client->isServer = 0;
    server_info->iAmServer = 0;
    
    p2p_ServerClients = NewP2PList (); // create a list to hold all the p2p information
    p2p_Clients = NewP2PList ();     // This holds the people I have connected to
    
    // is on is used to quickly kill an instance
    isOn = 1;
    
    // thread for p2p and client->server connections
    thrd_t p2p_thread;
    thrd_t client_thread;
    int logout = 0;
    
    // Create the P2P Server
    int p2pError;
    if ( p2pError = thrd_create(&p2p_thread, p2pHandler, (void *) NULL)) {
        perror("Error!");
    }
    
    // Create Client connection with the Server
    int clientError;
    if ( clientError = thrd_create(&client_thread, clientHandler, 
                                                              (void *) NULL)) {
        perror("Error!");
    }
    
    // Wait so the threads are Instantly closed
     while (1) {
        // Equivalent to `sleep(0.1)`
        usleep(100000);
    }
    return 0;
}

// This function is responsible for accepting information from the server
// It is started as a thread created in the clientHandler() function.
// It recieves responses from TIMEOUT messages and private messages used to
// Initiate p2p
// The server asks this function for its information so that p2p can be 
// initiated with the client.
// Input:   void *args_ : This struct is used to carry timeout data from the
//                        clientHandler(). Like the socket fd and some managment
//                        data.
// Output:  This function waits until it recieves information from the server to
//          act. 
//          InitiateP2P() - It also calls a thread to initiate p2p with another
//                          client. 
int timeoutHandler(void *args_) {
    
    thrd_t initiateP2P_thread;
    timeout data = (timeout) args_;
    int sock = data->fd;

    int alive = 1;
    int maxlen = 200, len = 0;
    char loggedInBuffer[maxlen];
    char* userBuffer = loggedInBuffer;

    int trigger = 0;

    while (alive) {

        len = read(sock , userBuffer , maxlen);
        userBuffer[len] = '\0';

        int newlines = 0;
        if (strncmp(userBuffer, "Timeout:", 8) == 0) {
            alive = 0;
            for (int i = 0; i <= len; i++) {
                printf("%c", userBuffer[i]);

            }
            printf("\n\r");
            isOn = 0;

            break;

        } else if (strncmp (userBuffer, "private!! Recv", 14) == 0) {
            connectionInfo->message = malloc(sizeof(char) * 200);
            char * message;
            message = strtok(userBuffer, " ");
            // I need to get the information from the User attempting to start
            // private.
            // Return the listening server of this client

            // Get the response from the Server
            int count = 0;
            for (message = strtok(NULL, " "); message != NULL; 
                                                  message = strtok(NULL, " ")) {
                if (count == 1) {
                    strcpy(connectionInfo->message, message);
                    strcat(connectionInfo->message, " ");
                } else if (count > 1) {
                    strcat(connectionInfo->message, message);
                    strcat(connectionInfo->message, " ");
                }
                count++;
            }

            // Initiate the Private Connection
            int InitiatePrivateError;
            if ( InitiatePrivateError = thrd_create(&initiateP2P_thread, 
                                initiateP2PHandler, (void *) connectionInfo)) {
            perror("Error!");
            }
            // connect to Client at the address above

        } else if (strncmp(userBuffer, "private!!", 9) == 0) {
            char buf[100]; 
            int length = 100;
            //Set up a message to send to the server that holds your conneciton
            // Information 
            snprintf(buf, length, "private!! %lu %u", 
                            p2p_address.sin_addr.s_addr, p2p_address.sin_port);
            length = strlen(buf) + 1;
            int sendbytes = send(sock, buf, length, 0);

        } else if (strlen(userBuffer) > 3) {
            printf("\r");
            char *sentence = "> ";
            for (int i = 0; i <= 2; i++) {
                printf("%c", sentence[i]);
            }
            for (int i = 0; i <= len; i++) {
                printf("%c", userBuffer[i]);
            }
            printf("\r\n");
        } else {
            printf("\r\n");
        }

    }
    return EXIT_SUCCESS;
}

// This function is the main Thread for the Client. It handles the transition
// from the login state to the loggedIn State. It also handles the sending of
// DATA from both the p2p-client and Server-Client.
// Input: 
//          void * args_ : is NULL here, this function relies on global 
//                          Variables
int clientHandler (void * args_) {

    //client_info connectionInfo = (client_info) args_;
    //this struct will contain address + port No
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;

    // http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
    inet_pton(AF_INET, connectionInfo->server_name, &server_address.sin_addr);

    // htons: port in network order format
    server_address.sin_port = htons(connectionInfo->server_port);

    // open a TCP stream socket using SOCK_STREAM, verify if 
             //socket successfuly opened
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not open socket\n");
        exit(1);
    }

    // TCP is connection oriented, a reliable connection
    // **must** be established before any data is exchanged
    //initiate 3 way handshake
    //verify everything ok
    if (connect(sock, (struct sockaddr*)&server_address,
                                                 sizeof(server_address)) < 0) {
        printf("could not connect to server\n");
        exit(1);
    }

    // prepare to receive
    int len = 0, maxlen = 200;
    char buffer[maxlen];
    char* pbuffer = buffer;
    //fsync(sock);
    // get input from the user
    char data_to_send[200];

    // Prepare a struct used in the TimeOut THREAD ABOVE
    timeout timeKeeper = malloc(sizeof(timeout));
    timeKeeper->fd = sock;
    timeKeeper->authorised = 0;

// timeKeeper->authorised == 0 when the client has no loggedin yet
    while (timeKeeper->authorised == 0) {


        //memset(pbuffer, 0, 100);
        len = recv(sock , pbuffer , 200 , 0);
        buffer[len] = '\0';

        if (timeKeeper->authorised == 0)
            printf(pbuffer);

        if (strcmp(pbuffer, "Success!") == 0) {
            printf("\r\n");            
            timeKeeper->authorised = 1;
            break;
        }

        //printf("Here!");
        fgets(data_to_send,200,stdin);
        strtok(data_to_send,"\n");
        send(sock, data_to_send, strlen(data_to_send)+1, 0);
        //fgets reads in the newline character in buffer, get rid of it
    }

///////////////////////////////////////////////////////////////////////////
//                                                                       //
//  Below this point the client is logged into the server                //
//                                                                       //
///////////////////////////////////////////////////////////////////////////
    char loggedInBuffer[maxlen];
    char* userBuffer = loggedInBuffer;
    // Conversation loop with the server
    len = 0;

    // Create a thread to deal with timeout
    thrd_t timeout_thread;

    // Create a thread for terminating by timeout
    int error;
    if ( error = thrd_create(&timeout_thread, timeoutHandler, 
                                                        (void *) timeKeeper)) {
        perror("Error!");
    }

    int begin = 0;
    while (timeKeeper->authorised == 1 && isOn) {
    //memset(data_to_send, '\0', maxlen);
    char *sentence = "> ";
    int length = strlen(sentence);
    for (int i = 0; i < length; i++) {

    printf("%c", sentence[i]);

    }
    

///////////////////////////////////////////////////////////////////////////
//                                                                       //
//                    Handle the stopPrivate command                     //
//                                                                       //
///////////////////////////////////////////////////////////////////////////
    if (strncmp(data_to_send, "stopprivate", 11) == 0) {
    // Close the connection that was established

    // Check if it is the client or the server that sends this
    // and send to the respective destination
        int found = 0;
        // First check that there is a username after the string
        char * message = strtok(data_to_send, " ");
        message = strtok(NULL, " ");
        if (message != NULL) {
            // Client
            char * stop = "stopprivate!!";
            int length = strlen(stop) + 1;
            snprintf(buffer, length, "%s", stop);
            char * user = malloc (sizeof (char) * 100);

            strcpy(user, message);

            // Find and populate the details of the user we want from the 
            // server list

            for (p2p_node node = p2p_ServerClients->first; node != NULL; 
                                                           node = node->next) {
                if (strcmp(node->username, user) == 0) {

                    found = 1;
                    // load the data from the list
                    server_info->serverfd = node->serverfd;

                    server_info->clientfd  = node->clientfd;
                }
            }
            for (p2p_node node = p2p_Clients->first; node != NULL; 
                                                           node = node->next) {
                if (strcmp(node->username, user) == 0) {

                found = 1;
                // load the data from the list
                server_info->serverfd = node->serverfd;

                server_info->clientfd  = node->clientfd;
                }
            }
            // Check in the connections list if there is none in the 
            //  serverClients
            if (found == 0) {
            // Check if the user is me
                if ((p2p_Clients->size >= 1 || p2p_ServerClients->size >= 1) && 
                    strcmp(user, server_info->myName) == 0) {
                    printf("Error! I can't stop communicating with myself\n");
                } else {
                    printf("Error! You need to create a private connection" 
                                                       "to that user first\n");
                }
            } 
            if (found == 1) {
                if (server_info->iAmServer == 0) {
                    sendPrivate(buffer);

                } else if (server_info->iAmServer == 1){
                    sendPrivateServer(buffer);

                }
                // Remove the entry from the list, because that client is no 
                // longer connected
                removeFromList (p2p_ServerClients, user);   // remove the name 
                removeFromList (p2p_Clients, user);         // Remove the name 
            }

        } else {
            printf("Please give a valid user to stop communicating with\n");
        }
    }
    
///////////////////////////////////////////////////////////////////////////
//                                                                       //
//                    Handle private command                             //
//                                                                       //
///////////////////////////////////////////////////////////////////////////
    if (strncmp(data_to_send, "private", 7) == 0) {
         // Check the server is active
        if (server_info->isActive == 1) {

            char * message = strtok(data_to_send, " ");
            char * user = malloc (sizeof (char) * 100);
            message = strtok(NULL, " ");
            strcpy(user, message);
            char * information = malloc (sizeof(char) * 200);
            
            // Check the user is valid.
            // Compare to two lists.
            
            // 1. the P2P list that you have connected with, that you are a 
            // client of
            
            // 2. The P2P list that you are the server for, people who have
            // connected to you
            int found = 0;
            if (p2p_ServerClients->size != 0) {
                for (p2p_node node = p2p_ServerClients->first; node != NULL; 
                                                           node = node->next) {
                    if (strcmp(node->username, user) == 0) {
                        found = 1;
                        // load the data from the list
                        server_info->serverfd = node->serverfd;

                        server_info->clientfd  = node->clientfd;
                    }
                }
            }
            if (p2p_Clients->size != 0) {
                for (p2p_node node = p2p_Clients->first; node != NULL; 
                                                           node = node->next) {
                    if (strcmp(node->username, user) == 0) {
                        found = 1;
                        // load the data from the list
                        server_info->serverfd = node->serverfd;

                        server_info->clientfd  = node->clientfd;
                    }
                }
            }
            if (found == 1) {
                strcat(information, "private message:(");

                strcat(information, server_info->myName);
                strcat(information, "): ");
                for (message = strtok(NULL, " "); message != NULL; 
                                                 message = strtok(NULL, " ")) {
                    strcat(information, message);
                    strcat(information, " ");
                }
                int messagelength = strlen(information) + 1;
                // int total = userlength + messagelengh;
                char *buffer = malloc (sizeof(char) * messagelength);
                snprintf(buffer, messagelength, "%s", information); 
                if (server_info->iAmServer == 1) {
                    sendPrivateServer(buffer);
                } else {
                    sendPrivate(buffer);
                }

            } else {
                // We have to check that that user is:

                // Not me:
                if (strcmp(server_info->myName, user) ==0) {
                    printf("You cannot connect with yourself\n");
                }

                // In the credentials list
                FILE * fp = fopen("credentials.txt", "r");
                int check = checkFileForUser(fp, user);
                if (check == 0) {
                    printf("Error! That user does not exist\n");
                }
                    fclose(fp);
            }   

        } else {
            printf("Error! No P2P connections are available\n");
        }
    }

///////////////////////////////////////////////////////////////////////////
//                                                                       //
//                    Handle the logout command                          //
//                                                                       //
///////////////////////////////////////////////////////////////////////////
        // Handle the Logout command
        if (strcmp(data_to_send, "logout") == 0) {
            // Make a stopprivate message to cancel the p2p connections you have
            char * stop = "stopprivate!!";
            int length = strlen(stop) + 1;
            snprintf(buffer, length, "%s", stop);
            
            // Loop over the clients that are connected to your server and
            // disconnect them
            for (p2p_node node = p2p_ServerClients->first; node != NULL; 
                                                           node = node->next) {
                server_info->serverfd = node->serverfd;
                server_info->clientfd  = node->clientfd;
                if (client->isServer == 1) {
                    sendPrivateServer(buffer);
                } else if (client->isServer == 0){
                    sendPrivate(buffer);
                }
            }
            
            // Destroy!!
            // Destroy the list of clients connected to your server
            p2p_node item = p2p_ServerClients->first;
            while (item != NULL) {
                p2p_node temp = item->next;
                free(item->username);
                free(item);
                item = temp; 
            }
            
            //free(p2p_ServerClients);
            
            // Loop over the Servers You have connected to with your p2p client
            // disconnect them
            for (p2p_node node = p2p_Clients->first; node != NULL; 
                                                           node = node->next) {
                server_info->serverfd = node->serverfd;
                server_info->clientfd  = node->clientfd;
                if (client->isServer == 1) {
                    sendPrivateServer(buffer);
                } else if (client->isServer == 0){
                    sendPrivate(buffer);
                }
                // Remove the name from the list
            }
            // Destroy!!
            // destroy the list of servers
            p2p_node item2 = p2p_Clients->first;
            while (item2 != NULL) {
            p2p_node temp = item2->next;
            free(item2->username);
            free(item2);
            item2 = temp; 

            }
            //free(p2p_Clients);
            
            // Give a logout message
            printf("LogoutOut\n");
            break;
        }
        fgets(data_to_send,300,stdin);
        strtok(data_to_send,"\n");
        send(sock, data_to_send, strlen(data_to_send)+1, 0);
    }
    // Exit, otherwise we will be caught in an infinite loop
    exit(1);

}

///////////////////////////////////////////////////////////////////////////
//                                                                       //
//      Create a Server on the Client side for p2p transfer              //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

// This function is responsible for Setting up the listening server that waits
// for p2p connections. It attempts to find any available port > 12345.
// Then listens and awaits connection
// Input:
//          void * args_: this is NULL here, extensive use of global variables
int p2pHandler (void * args_) {
    
    //client_info connectionInfo = (client_info) args_;
    thrd_t p2pServer_thread;

    // socket address used for the server
    int P2P_PORT = 12345;


    // zero the memory
    memset(&p2p_address, 0, sizeof(p2p_address));
    p2p_address.sin_family = AF_INET;

    // htons: host to network short: transforms a value in host byte
    // ordering format to a short value in network byte ordering format
    // Network byte ordering is little-endian format
    p2p_address.sin_port = htons(P2P_PORT);

    // htonl: host to network long: same as htons but to long
    p2p_address.sin_addr.s_addr = htonl(INADDR_ANY);

    // create a TCP socket (using SOCK_STREAM), creation returns -1 on failure
    int listen_sock;
    if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("could not create listen socket\n");
        return 1;
    }

    // bind it to listen to the incoming connections on the created server
    // address, will return -1 on error
    int p2pPort = 0;
    // Loop and try find an openport
    while (p2pPort == 0) {
        if ((bind(listen_sock, (struct sockaddr *)&p2p_address,
        sizeof(p2p_address))) < 0) {
            P2P_PORT = P2P_PORT + 1;
            p2p_address.sin_port = htons(P2P_PORT);
        } else {
            p2pPort = 1;
        }
    }

    // Wait and listen for a connection
    int wait_size = 10;  // maximum number of waiting clients
    if (listen(listen_sock, wait_size) < 0) {
        printf("could not open socket for listening\n");
        return 1;
    }


    struct sockaddr_in client_address;
    socklen_t client_address_len=sizeof(client_address);
    
    // Wait for connection
    int sock;
    while (1) {

        if (sock = accept(listen_sock, (struct sockaddr *)&client_address,
                                                        &client_address_len)) {
            inet_ntoa(client_address.sin_addr);

            server_info->isActive = 1;
            client->isActive = 1;
            client->isServer = 1;
            // Create the thread
            int error;
            server_info->serverfd = sock;
            if (error = thrd_create(&p2pServer_thread, p2pServer_handler, 
                                                       (void *) server_info)) {
                printf("Error Creating thread: %d\n", error);
            }
        }
    }
    ///////////////////////////////////////////////////////////////////////////
    //                                                                       //
    //  End of Server creation for P2P communication                         //
    //                                                                       //
    ///////////////////////////////////////////////////////////////////////////
    server_info->isActive = 0;
    //client->isActive = 0;
    client->isServer = 0;

    // close the socket
    close(sock);
    //exit(1);
    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//                                                                           //
//              Receive on the P2P socket once connections are made          //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
// This function receives information from the client that has connected to the 
// p2p server. It is a thread that is created when a client connects to the 
// server.
// Input:
//          void *info : This contains the struct that contains the data of 
//                       the client that connected to the server/

int p2pServer_handler (void *info) {

    p2pServer serverInfo = (p2pServer) info;
    char userBuffer[100];
    char* loginBuffer = userBuffer;

    char data_to_send[100];

    int n = 0;

    while (server_info->isActive == 1) {
        //  if (server_info->tempFreeze == 1) {
        memset(loginBuffer, 0, 100);
        n = recv(server_info->serverfd, loginBuffer, 100,  0);
        //printf("n: %d\n", n);
        loginBuffer[n] = '\0';

///////////////////////////////////////////////////////////////////////////////
///                                                                         ///
///     This Part is The Application protocol I have designed to pass       ///
///      critical Information                                               ///
///                                                                         ///
///////////////////////////////////////////////////////////////////////////////
        if (strncmp(loginBuffer, "###data###", 10) == 0) {
            //printf("Loggin Buffer: %s\n", loginBuffer);
            char * message = strtok(loginBuffer, " ");
            message = strtok(NULL, " ");
            // Tag for client is server

            int length = strlen(message) + 1;
            server_info->userToMessage = malloc (sizeof(char) * length);
            strcpy(server_info->userToMessage, message);
            //printf("ServerName: %s\n", server_info->serverName);
            printf("User: %s, initiated a private connection with you\n", 
                                                   server_info->userToMessage);

            // Get my name
            message = strtok(NULL, " ");
            message = strtok(NULL, " ");
            server_info->myName = malloc(sizeof(char) * strlen(message) + 1);
            strcpy(server_info->myName, message);
            message = strtok(NULL, " ");
            //message = strtok(NULL, " ");
            server_info->iAmServer = atoi(message);
            message = strtok(NULL, " ");
            server_info->clientfd = atoi(message); // set the server fd

            ///////////////////////////////////////////////////////////////
            ///          Send the server Socket in a reply!!            ///
            ///////////////////////////////////////////////////////////////
            // This is part of the Application layer protocol I have designed
            char *string1 = "#2#data#2# ";
            length = strlen(string1) + sizeof(server_info->serverfd);
            char * reply = malloc (sizeof(char) * length);
            strcpy(reply, "#2#data#2#");
            strcat(reply, " ");
            sprintf(reply, "%s%d", reply, server_info->serverfd);
            send(server_info->serverfd, reply, strlen(reply)+1, 0);

            //////////////////////////////////////////////////////////////////
            //                                                              //
            //  As the server I want to store this information in a list    //
            //  These are the people who connect to me                      //
            //////////////////////////////////////////////////////////////////
            // I store the p2pserverfd and the p2pcleintfd in this struct
            // as well as the clients username, so we have a reference to them
            P2PNodeInsert (p2p_ServerClients, server_info->userToMessage, 
                                server_info->serverfd, server_info->clientfd);
        
        
        //////////////////////////////////////////////////////////////////////
        //                                                                 //
        // If the stopprivate command has been received by the client      //
        /////////////////////////////////////////////////////////////////////
        } else if (strncmp(loginBuffer, "stopprivate!!", 11) == 0) {
            client->isServer = 0;
            printf("%s : Has stopped communicating privately\n", 
                                     server_info->userToMessage, loginBuffer);

            removeFromList (p2p_ServerClients, server_info->userToMessage); 
            removeFromList (p2p_Clients, server_info->userToMessage);
            removeFromList (p2p_ServerClients, server_info->myName); 
            removeFromList (p2p_Clients, server_info->myName);
            free(server_info->myName);
            free(server_info->userToMessage);
            break;
        } else {
            printf("%s\n", loginBuffer);
        }
    }

    return EXIT_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
///                                                                         ///
/// This function is responsible for initiating a p2p with a p2p server     ///
///                                                                         ///
///////////////////////////////////////////////////////////////////////////////
// This function handles the response received from the server.
// THe server sends the credentials for the client to connect with the other 
// client. This client then connects via p2p with the client who the user wants
// to start a connection with.
// Input:
//          void * info: IS null here.
//          connectionInfo : This struct is used to carry the information to
//                           The client wishing to connect to the other client.
//
int initiateP2PHandler (void *info) {

    //prepare server credentials
    int count = 0;
    
    // Get the information sent by the server
    char * message = strtok(connectionInfo->message, " ");
    unsigned long server_name;
    short port;
    struct hostent *hostaddr;
    const char *serverName = "0";
    
    // Create variables to store my own name and the name of the server
    // THis information is collected from connectionInfo->message;
    server_info->userToMessage = malloc (sizeof (char) * strlen(message) + 1);
    strcpy(server_info->userToMessage, message);
    
    message = strtok(NULL, " ");
    
    server_info->myName = malloc (sizeof(char) * strlen(message) + 1);
    strcpy(server_info->myName, message);
    
    // Collect the port information
    for (message = strtok(NULL, " "); message != NULL; 
                                                 message = strtok(NULL, " ")) {
        connectionInfo->p2p_port = atoi(message);
    }

    //this struct will contain address + port No
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    
    // http://beej.us/guide/bgnet/output/html/multipage/inet_ntopman.html
    //printf("Servername: %s\n", connectionInfo->p2pServer_name);
    inet_pton(AF_INET, serverName, &server_address.sin_addr);

    //server_address.sin_addr.s_addr = connectionInfo->p2pServer_name;
    // htons: port in network order format
    server_address.sin_port = connectionInfo->p2p_port;
    
    // open a TCP stream socket using SOCK_STREAM, verify if socket 
                                                    //successfuly opened
    int sock;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("could not open socket\n");
        return 1;
    }
    
    // TCP is connection oriented, a reliable connection
    // **must** be established before any data is exchanged
    //initiate 3 way handshake
    //verify everything ok
    if (connect(sock, (struct sockaddr*)&server_address,
                sizeof(server_address)) < 0) {
        perror("could not connect to server\n");
        return 1;
    }
    
    // Welcome 
    printf("Initiated a Private Connection with: %s\n", 
                                                   server_info->userToMessage);
    // prepare to receive
    int len = 0, maxlen = 100;
    char buffer[maxlen];
    char* pbuffer = buffer;
    //fsync(sock);
    // get input from the user
    char data_to_send[100];
    server_info->isActive = 1;
    
    server_info->clientfd = sock;
    // initation message:
    // send the server its name and information explaining who is server
    // and who is client
    strcpy(data_to_send, "###data### ");
    strcat(data_to_send,  server_info->myName);
    strcat(data_to_send, " ");
    strcat(data_to_send, "0"); // iAmServer == 0;
    strcat(data_to_send, " ");
    strcat(data_to_send, server_info->userToMessage);
    strcat(data_to_send, " ");
    strcat(data_to_send, "1");  // iAmServer == 1;
    strcat(data_to_send, " ");
    // send the fd of the client to the server
    sprintf(data_to_send, "%s%d", data_to_send, server_info->clientfd);
    strtok(data_to_send, "\n");
    send(sock, data_to_send, strlen(data_to_send)+1, 0); //SEND DATA!
    
    client->fd = sock;
    client->userToMessage = malloc (sizeof(char) * 100);
    strcpy(client->userToMessage, server_info->userToMessage);
    
    // Create a client listener thread to listen to data sent by the server
    // This thread is where we save the server and client info into the list
    thrd_t p2pClientListener_thread;
    client->isActive = 1;
    int p2pListenerError;
     if (p2pListenerError = thrd_create(&p2pClientListener_thread,
                                p2pClientListener_handler, (void *) client)) {
        printf("Error Creating thread");
     }

    return EXIT_SUCCESS;
}

// This function is responsible for listening for data sent from the p2pserver
// to the p2pclient.
// It also is repsonsible for adding newly added clients into the connectionlist
// so that this user knows who he/she is talking too
int p2pClientListener_handler (void * info) {
    
    char userBuffer[100];
    char* loginBuffer = userBuffer;
    
    char data_to_send[100];
   
    int n = 0;
    
    while (client->isActive == 1) {
        memset(loginBuffer, 0, 100);
        //printf("LoginFlag: %d\n", getLoginFlagInUserList (users, ubuffer));
        n = recv(server_info->clientfd, loginBuffer, 100,  0);
        loginBuffer[n] = '\0';
        
        if (strncmp(loginBuffer, "stopprivate!!", 11) == 0) {
            printf("%s : Has stopped communicating privately\n", 
                                    server_info->userToMessage, loginBuffer);
            //server_info->isActive = 0;
            //client->isActive = 0;
            removeFromList (p2p_ServerClients, server_info->userToMessage); 
            removeFromList (p2p_Clients, server_info->userToMessage);
            break;
            // Collect the second response from the server, giving the details
            //  we need to create our list
        } else if (strncmp(loginBuffer, "#2#data#2#", 10) == 0) {
            //printf("LoginBuffer: %s\n", loginBuffer);
            char * message = strtok(loginBuffer, " ");
            message = strtok(NULL, " ");
            server_info->serverfd = atoi(message);
            //printf("Server Socket: %d\n", server_info->serverfd);
            server_info->iAmServer = 0;
            ///////////////////////////////////////////////////////////////////
            //                                                               //
            //  Create the connections list for this client                  //
            ///////////////////////////////////////////////////////////////////
            
            P2PNodeInsert (p2p_Clients, server_info->userToMessage, 
                                server_info->serverfd, server_info->clientfd);
        } else {
            printf("%s\n", loginBuffer);
        }
    }
    
    
    return EXIT_SUCCESS;
}

// This function is for the server to send to the client
int sendPrivate(char *message) {
        
    int len = 0, maxlen = 100;
    char buffer[maxlen];
    char* pbuffer = buffer;
    // get input from the user
    char data_to_send[100];
        
 
   //printf("Here!");
    strcpy(data_to_send, message);
    strtok(data_to_send,"\n");
    send(server_info->clientfd, data_to_send, strlen(data_to_send)+1, 0);
    //fgets reads in the newline character in buffer, get rid of it
  
    return EXIT_SUCCESS;
}

// This function is for the client to send to the server
int sendPrivateServer(char *message) {
        
    int len = 0, maxlen = 100;
    char buffer[maxlen];
    char* pbuffer = buffer;
    //fsync(sock);
    // get input from the user
    char data_to_send[100];
        
 
   //printf("Here!");
    strcpy(data_to_send, message);
    strtok(data_to_send,"\n");
    send(server_info->serverfd, data_to_send, strlen(data_to_send)+1, 0);
    //fgets reads in the newline character in buffer, get rid of it
  
    return EXIT_SUCCESS;
}

// Checks if a user is in the credentials file
int checkFileForUser(FILE * fp, char * username) {
    // check the name entered is on the list of users
    int success = 0;
    int num;
    char *tmpUser = malloc (sizeof (char) * 100);
    char *tmpPass = malloc (sizeof (char)* 100);
    while ((num = fscanf(fp, "%s %s", tmpUser, tmpPass)) != EOF) {
    //fgetc(fp); // read the newline
    
        if ((strcmp(tmpUser, username) == 0)) {
            return 1;
        } 
    }
    free(tmpUser);
    free(tmpPass);
    return 0;
}

