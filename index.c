#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

#define BUFLEN 100 // Maximum UDP message size
#define NAMESIZ 20 // Maximum file or username size
#define MAX_NUM_CON 200 // Maximum number of content entries

// Data structure for a linked list node representing a user entry
typedef struct entry {
  char user_name[NAMESIZ];
  char ip[16];
  char port[7];
  short token;
  struct entry * next;
} ENTRY;

// Data structure representing a content list
typedef struct {
  char name[NAMESIZ]; // Content Name
  ENTRY * head;
} LIST;

// Global variables
LIST list[MAX_NUM_CON];
int max_index = 0;

// Data structure for UDP protocol communication
typedef struct {
  char type;
  char data[BUFLEN];
} PDU;
PDU tpdu;

void search(int, char * , struct sockaddr_in * );
void registration(int, char * , struct sockaddr_in * );
void deregistration(int, char * , struct sockaddr_in * );

// Socket address for communication
struct sockaddr_in fsin;

// Function prototypes & UDP Content Indexing Service
int main(int argc, char * argv[]) {
  struct sockaddr_in sin, * p_addr; // the from address of a client	
  ENTRY * p_entry;
  char * service = "10000"; // service name or port number	
  char name[NAMESIZ], user_name[NAMESIZ];
  int alen = sizeof(struct sockaddr_in); // from-address length		
  int s, n, i, len, p_sock; // socket descriptor and socket type    
  int pdulen = sizeof(PDU);
  struct hostent * hp;
  PDU rpdu;
  int j = 0;

  PDU spdu;

  // Initialize content lists
  for (n = 0; n < MAX_NUM_CON; n++)
    list[n].head = NULL;

  // Parse command-line arguments for port number  
  switch (argc) {
  case 1:
    break;
  case 2:
    service = argv[1];
    break;
  default:
    fprintf(stderr, "Not Correct Arugments \n use: server [host] [port]\n");
  }

  // Configure server address
  memset( & sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons((u_short) atoi(service));   // Map service name to port number 

  // Create UDP socket
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    fprintf(stderr, "Failed to create socket\n");
    exit(1);
  }

  // Bind socket to the specified port 
  if (bind(s, (struct sockaddr * ) & sin, sizeof(sin)) < 0)
    fprintf(stderr, "Failed to bind to port %s \n", service);

  // Main server loop to handle client requests
  while (1) {
    // Receive data from a client
    if ((n = recvfrom(s, & rpdu, pdulen, 0, (struct sockaddr * ) & fsin, & alen)) < 0) {
      printf("Error recieving data n=%d\n", n);
    }
    //Process requests based on the message type			
    if (rpdu.type == 'R') {
      printf("Registering content\n");
      registration(s, rpdu.data, & fsin);//call registration function with socket, data, client address
      printf("%d\n", s);
    }

    // Search Content 'S'		
    if (rpdu.type == 'S') {
      printf("Searching for content\n");
      search(s, rpdu.data, & fsin);//call search function with socket, data, client address
    }

    //List All Content 'O' 
    if (rpdu.type == 'O') {
      printf("List content\n");
      for (j = 0; j < max_index; j++) {
        if (list[j].head != NULL) {
          PDU spdu;
          spdu.type = 'O';
          strcpy(spdu.data, list[j].name);
          printf("%s\n", list[j].name);
          (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
        }
      }
      
      // Send acknowledgment for end of content listing
      PDU opdu;
      opdu.type = 'A';
      (void) sendto(s, & opdu, sizeof(opdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
    }

     // Deregistration request		
    if (rpdu.type == 'T') {
      printf("de-register\n");
      deregistration(s, rpdu.data, & fsin);//call de-registration function with socket, data, client address
    }
  }
  return;
}

// Function to search for content in the server's list
// s is the socket address
// *data is the data (username and filename) to search for
// *addr is the address of the client sending the request
void search(int s, char * data, struct sockaddr_in * addr) {
  int j;                   // Loop counter
  int found = 0;           // Flag to indicate if the content is found
  int used = 999;          // Variable to track the least used token
  ENTRY *use;              // Pointer to the least used entry
  ENTRY *head;             // Pointer to traverse the linked list
  int pdulen = sizeof(PDU); // Size of the PDU structure
  PDU spdu;                // PDU structure to hold the response
  char user_name[20];      // Variable to store the extracted username
  char ouput[100];         // Buffer to format the output data
  char fileName[20];       // Variable to store the extracted filename
  char rep[2] = ",";       // Delimiter for splitting the received data

  // Split the received data into username and filename
  strcpy(user_name, strtok(data, rep));
  strcpy(fileName, strtok(NULL, rep));

  // Iterate through the list to search for the requested content
  for (j = 0; j < max_index; j++) {
    printf("%s\n", list[j].name);// Print the current content name in the list
    
    // Check if the file matches and it has entries in the linked list
    if (strcmp(list[j].name, fileName) == 0 && (list[j].head != NULL)) {
      found = 1;             // Mark the content as found
      head = list[j].head;   // Set the pointer to the head of the linked list

      // Traverse the linked list to find the least used entry 
      while (head != NULL) {
        if (head -> token < used) {
          used = head -> token;   // Update least used token
          use = head;             // Store the pointer to the least used entry
        }
        head = head -> next; // Move to the next entry in the linked list 
      }
      break; // Exit the loop if the content is found
    }
  }

  // Prepare the response based on whether the content is found
  if (found == 1) {
    spdu.type = 'S'; // Set response type to 'S' (success)

    // Format the output data with username, filename, IP, and port
    strcpy(ouput, use -> user_name);
    strcat(ouput, ",");
    strcat(ouput, fileName);
    strcat(ouput, ",");
    strcat(ouput, use -> ip);
    strcat(ouput, ",");
    strcat(ouput, use -> port);
    printf("%s\n", ouput); // print formatted data

    strcpy(spdu.data, ouput); // Copy the output data to the PDU
    use -> token++;  // Increment the usage token for the entry

    // Send the response to the client
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  } else {
    spdu.type = 'E';      // Set response type to 'E' (error)
    strcpy(spdu.data, "File not found");//Set the Error message
    // Send the error response to the client
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
  printf("Ending\n");
}

// Function to deregister content from the server
// s is the socket address
// *data is the data (username and filename) to deregister
// *addr is the address of the client requesting deregistration
void deregistration(int s, char * data, struct sockaddr_in * addr) {
  int j;                      // Loop counter
  int use = -1;               // Index of the found entry
  ENTRY *prev;                // Pointer to the previous entry in the linked list
  ENTRY *head;                // Pointer to traverse the linked list
  int listIndex = 0;          // Index of the current entry in the list
  PDU spdu;                   // PDU structure to hold the response
  char rep[2] = ",";          // Delimiter for splitting the data
  char user_name[20];         // Variable to store the extracted username
  char file[20];              // Variable to store the extracted filename
  
  printf("Deregistering %s\n", data);//print data being processed

  // Split the received data into username and filename
  strcpy(user_name, strtok(data, rep));
  strcpy(file, strtok(NULL, rep));

  // Iterate through the list to find the file
  for (j = 0; j < max_index; j++) {
    if (strcmp(list[j].name, file) == 0) {
      head = list[j].head;    //point to the head of linked list
      prev = list[j].head;    //initialize previous to the head of linked list
      listIndex = 0;          //reset index

      // Traverse the linked list to find the user
      while (head != NULL) {
        printf("User list: %s\n", head -> user_name);//print usernmae in the list
        printf("User given: %s\n", user_name);//print given username

        // Check if the username matches
        if (strcmp(head -> user_name, user_name) == 0) {
          printf("Compare username success\n");
          printf("List index: %d\n", listIndex);
          use = listIndex; // Store the index of the found entry

          // Update the linked list to remove the entry
          if (listIndex == 0) {
            list[j].head = head -> next; //update head to the next entry 
          } else {
            prev -> next = head -> next; //bypass the currnt entry in the list
          }
          break; // exit loop if entry is found
        }
        listIndex++;          //increment list index
        prev = head;          // update prev to current entry
        head = head -> next;  //move to next entry in the list
      }
      break;//exit if file is found
    }
  }

  // Prepare the response based on whether the entry was removed
  if (use != -1) {
    spdu.type = 'A';           // Set response type to 'A' (acknowledgment)
    strcpy(spdu.data, "Done"); // Set the success message
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  } else {
    spdu.type = 'E';             // Set response type to 'E' (error)
    strcpy(spdu.data, "Failed"); // Set the error message
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
}

// s is socket address
// *data is the data name
// *addr is the address of the client sending/requesting
// Register Content to the Server
void registration(int s, char * data, struct sockaddr_in * addr) {
  // Register the content and the server of the content
  ENTRY * new = NULL;
  new = (ENTRY * ) malloc(sizeof(ENTRY));//allocate memory for a new entry 
  int j;
  ENTRY * head;
  int used = 999;

  int duplicateUser = 0;
  char rep[2] = ",";
  PDU spdu;
  char fileName[20];
  int found = 0;
  char * ip = inet_ntoa(fsin.sin_addr); //convert IP to string

  printf("Sending ip address %s\n", ip);
  printf("Socket %d\n", s);
  printf("Data %s\n", data);
  // get user, file name, and port from the recieved 
  strcpy(new -> user_name, strtok(data, rep));
  strcpy(fileName, strtok(NULL, rep));
  strcpy(new -> port, strtok(NULL, rep));
  strcpy(new -> ip, ip);

  printf("Stored Ip %s\n", new -> ip);
  new -> token = 0;// initialize token count
  new -> next = NULL;//initialize the next pointer to null
  // iterate list until you find name == data
  for (j = 0; j < max_index; j++) {
    if (strcmp(list[j].name, fileName) == 0) {
      head = list[j].head;
      found = 1;
      // loop through the linked list until you get to the end to add content to tail
      while (head != NULL) {
        // stops seg faults when registering content after full deregister
        if (head -> next == NULL) {
          break;
        }
        // check if username is already used
        if (strcmp(head -> user_name, data) == 0) {
          duplicateUser = 1;
          break;
        }
        head = head -> next;
      }
      if (head == NULL) { // First Content on Linked List Initialize
        list[j].head = new;
      } else { // Other Cases (add new entry to the end of the list)
        head -> next = new;
      }
      break;
    }
  }
  //File not Found
  if (found == 0) {
    strcpy(list[max_index].name, fileName);
    list[max_index].head = new;
    max_index++;
  }
  //handle duplicate user registration
  if (duplicateUser == 1) {
    printf("Duplicate\n");
    spdu.type = 'E';
    strcpy(spdu.data, "Duplicate user name"); // Duplicate File Name?
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));

  } else {
    printf("Unique\n");
    // Otherwise send acknowledgement 'A'
    spdu.type = 'A';
    strcpy(spdu.data, "Done");
    (void) sendto(s, & spdu, sizeof(spdu), 0, (struct sockaddr * ) & fsin, sizeof(fsin));
  }
}
