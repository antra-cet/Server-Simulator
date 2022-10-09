#include <stdio.h>
#include <stdlib.h>    
#include <unistd.h>     
#include <string.h>     
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>     
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

// Defining the strings needed and values used to allocate memory
#define HOST "34.241.4.235"
#define PORT 8080
#define MAX_CLIENT_LEN 100
#define MAX_PSSWD_LEN 100
#define NAME_LEN 100

#define REGISTER "/api/v1/tema/auth/register"
#define REGISTER_PAYLOAD "application/json"
#define LOGIN "/api/v1/tema/auth/login"
#define ENTER_LIBRARY "/api/v1/tema/library/access"
#define GET_BOOKS "/api/v1/tema/library/books"
#define LOGOUT "/api/v1/tema/auth/logout"

// Used two global variables to verify if the client
// is logged in and to verify if he entered the library
int loggedIn = 0;
int enteredLibrary = 0;

/* For register command */
void registerClient(int sockfd, char *username, char *password) {
    // Defining the JSON objects
    JSON_Value *value;
    JSON_Object *object;
    char *serialized_string;

    // Initializing their values
    value = json_value_init_object();
    object = json_value_get_object(value);

    // Setting the fields
    json_object_set_string(object, "username", username);
    json_object_set_string(object, "password", password);
    serialized_string = json_serialize_to_string_pretty(value);

    // Use the POST request
    char *post_request = compute_post_request(HOST, REGISTER, REGISTER_PAYLOAD, &serialized_string, 1, NULL, 0, NULL);

    // Send the command and recieve a string from the server
    send_to_server(sockfd, post_request);
    char *server_response = receive_from_server(sockfd);

    // Printing the message from the server
    char *p = strtok(server_response, " ");
    p = strtok(NULL, " ");
    int error = atoi(p);

    while (p != NULL) {
        p = strtok(NULL, " ");
    }

    if (error == 201) {
        printf("Your account was created!\n");
    } else {
        if (error == 429) {
            printf("Too many requests!\n");
        } else {
            printf("Username already used in another account!\n");
        }
    }

    // Freeing values
    json_free_serialized_string(serialized_string);
    json_value_free(value);
}

/* For login command */
int login(int sockfd, char *username, char *password,  char **cookie) {
    // Defining the JSON objects
    JSON_Value *value;
    JSON_Object *object;
    char *serialized_string;

    // Initializing their values
    value = json_value_init_object();
    object = json_value_get_object(value);

    // Setting the fields
    json_object_set_string(object, "username", username);
    json_object_set_string(object, "password", password);
    serialized_string = json_serialize_to_string_pretty(value);

    // Use the POST request
    char *post_request = compute_post_request(HOST, LOGIN, REGISTER_PAYLOAD, &serialized_string, 1, NULL, 0, NULL);

    // Send the command and recieve a string from the server
    send_to_server(sockfd, post_request);
    char *server_response = receive_from_server(sockfd);

    // Verification if the response recieved is empty
    int ok = (basic_extract_json_response(server_response) == NULL);

    // Setting the cookie
    char *p = strtok(server_response, " ");
    p = strtok(NULL, " ");
    int error = atoi(p);

    while (p != NULL) {
        char *found_cookie = strstr(p, "connect.sid");
        if (found_cookie) {
            char *c = strtok(found_cookie, ";");
            if (ok) {
                strcpy(*cookie, c);
            }
        }

        p = strtok(NULL, " ");
    }

    // Printing errors or OK
    if (error == 200) {
        printf("Logged into your account!\n");
        return 1;
    }

    if (error == 429) {
        printf("Too many requests!\n");
        return 0;
    }

    printf("Invalid username/password!\n");
    return 0;
}

/* For enter_library command */
int enter_library(int sockfd, char  *cookie, char **jwt) {
    // Setting the get request and sending it to the server
    char *get_request = compute_get_request(HOST, ENTER_LIBRARY, NULL, &cookie, 1, NULL);
    send_to_server(sockfd, get_request);

    // Recieving the response from the server
    char *server_response = receive_from_server(sockfd);
    strcpy(*jwt, server_response);

    // Verifying if entered
    char *p = strtok(server_response, " ");
    p = strtok(NULL, " ");
    if (atoi(p) == 200) {
        printf("Entered library!\n");
        return 1;
    } else {
        printf("An error occured!\n");
        return 0;
    }   
}

/* For get_books command */
void get_books(int sockfd, char *token, char *cookie) {
    // Setting the get request and sending it to the server
    char *get_request = compute_get_request(HOST, GET_BOOKS, NULL, NULL, 0, token);
    send_to_server(sockfd, get_request);

    // Recieving the response from the server
    char *server_response = receive_from_server(sockfd);
    char *p = strtok(server_response, "\n");
    for (int i = 0; i < 15; i++) {        
        p = strtok(NULL, "\n");
    }

    // Printing the books
    printf("%s\n", p);
}

/* For get_book command */
void get_book(int sockfd, int id, char *token) {
    // Setting the request string
    char *get_book_id = calloc(LINELEN, sizeof(char));
    sprintf(get_book_id, "%s/%d", GET_BOOKS, id);

    // Setting the get request and sending it to the server
    char *get_request = compute_get_request(HOST, get_book_id, NULL, NULL, 0, token);
    send_to_server(sockfd, get_request);

    // Recieving the response from the server
    char *server_response = receive_from_server(sockfd);

    // Using strtok to verify if the book was found and printing it
    char *string = calloc(LINELEN, sizeof(char));
    strcpy(string, server_response);
    char *p = strtok(string, "\n");

    if (strstr(p, "200")) {
        p = strtok(NULL, "\n");
        for (int i = 1; i < 15; i++) {  
            p = strtok(NULL, "\n");
        }

        printf("%s\n", p);
        free(get_book_id);
        free(string);

        return;
    }

    printf("An error occured!\n %s\n", p);
    free(get_book_id);
    free(string);
}

/* For add_book command */
void add_book(int sockfd, struct book *new_book, char *token, char *cookie) {
    // Defining the JSON objects
    JSON_Value *value;
    JSON_Object *object;
    char *serialized_string;

    // Initializing their values
    value = json_value_init_object();
    object = json_value_get_object(value);

    // Setting the fields
    json_object_set_string(object, "title", new_book->title);
    json_object_set_string(object, "author", new_book->author);
    json_object_set_string(object, "genre", new_book->genre);
    json_object_set_number(object, "page_count", new_book->page_count);
    json_object_set_string(object, "publisher", new_book->publisher);
    serialized_string = json_serialize_to_string_pretty(value);

    // Sending the post request
    char *post_request = compute_post_request(HOST, GET_BOOKS, REGISTER_PAYLOAD, &serialized_string, 1, &cookie, 1, token);
    send_to_server(sockfd, post_request);

    // Recieving the response from the server
    char *server_response = receive_from_server(sockfd);

    // Printing the message
    char *string = calloc(LINELEN, sizeof(char));
    strcpy(string, server_response);
    char *p = strtok(string, "\n");
    if (strstr(p, "200")) {
        printf("Book added!\n");
    } else {
        printf("An error occured!\n %s\n", p);
    }
}

/* For delete_book command */
void delete_book(int sockfd, int id, char *token) {
    // Setting the request string
    char *get_book_id = calloc(LINELEN, sizeof(char));
    sprintf(get_book_id, "%s/%d", GET_BOOKS, id);

    // Sending the delete request
    char *get_request = compute_delete_request(HOST, get_book_id, NULL, NULL, 0, token);
    send_to_server(sockfd, get_request);

    // Recieving the response
    char *server_response = receive_from_server(sockfd);

    // Printing the message
    char *string = calloc(LINELEN, sizeof(char));
    strcpy(string, server_response);
    char *p = strtok(string, "\n");

    if (strstr(p, "200")) {
        printf("Book deleted!\n");
        free(get_book_id);
        free(string);

        return;
    }

    printf("An error occured!\n %s\n", p);
    free(get_book_id);
    free(string);
}

/* For logout command */
int logout(int sockfd, char *cookie) {
    // Setting the get request and sending it to the server
    char *get_request = compute_get_request(HOST, LOGOUT, NULL, &cookie, 1, NULL);
    send_to_server(sockfd, get_request);

    // Recieving the response
    char *server_response = receive_from_server(sockfd);

    // Printing the message
    char *string = calloc(LINELEN, sizeof(char));
    strcpy(string, server_response);
    char *p = strtok(string, "\n");
    if (strstr(p, "200")) {
        printf("Logged out!\n");
        free(string);

        return 0;
    }

    printf("An error occured!\n %s\n", p);
    free(string);
    return 1;
}

int main(int argc, char *argv[]) {
    // Allocating memory for the fields
    char *cookie = calloc(LINELEN, sizeof(char));
    char *token = calloc(LINELEN, sizeof(char));
    char *jwt = calloc(LINELEN, sizeof(char));
    int sockfd;
    char end_line;

    // Begginning statement
    printf("=============== Type a command ===============\n");
    printf("-- register\n");
    printf("-- login\n");
    printf("-- enter_library\n");
    printf("-- get_books\n");
    printf("-- get_book\n");
    printf("-- add_book\n");
    printf("-- delete_book\n");
    printf("-- logout\n");
    printf("-- exit\n");
    printf("\n");

    // The while loop
    while(1) {
        // Allocating memory for the command then reading it
        char *command = malloc(NAME_LEN *  sizeof(char));
        fgets(command, NAME_LEN, stdin);
        command[strlen(command) - 1] = '\0';

        if (strcmp(command, "exit") == 0) {
            // If the command is exit, then free the
            // strings and printing exit
            free(command);

            printf("\n\n");
            free(cookie);
            free(jwt);
            free(token);
            printf("================== Exiting ==================\n");
            return 0;
        }

        if (strcmp(command, "register") == 0) {
            // If the command is register

            // Open the connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Reading the credidentials
            char *username = malloc (MAX_CLIENT_LEN * sizeof(char));
            printf("username=");
            scanf("%s", username);
            scanf("%c", &end_line);

            char *password = malloc(MAX_PSSWD_LEN * sizeof(char));
            printf("password=");
            scanf("%s", password);
            scanf("%c", &end_line);

            // Calling the register function
            registerClient(sockfd, username, password);

            // Closing the socket and freeing the command space
            printf("\n\n");
            close(sockfd);
            free(command);
            continue;
        }

        if (strcmp(command, "login") == 0) {
            // If the command is login

            // Verify login
            if (loggedIn == 1) {
                printf("You are already logged in.\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Opening the connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Reading the credidentials
            char *username = malloc(MAX_CLIENT_LEN * sizeof(char));
            printf("username=");
            scanf("%s", username);
            scanf("%c", &end_line);

            char *password = malloc(MAX_PSSWD_LEN * sizeof(char));
            printf("password=");
            scanf("%s", password);
            scanf("%c", &end_line);

            // Setting the cookie on NULL and calling login function
            strcpy(cookie, "");
            loggedIn = login(sockfd, username, password, &cookie);

            // Closing the socket and freeing the command space
            printf("\n\n");
            close(sockfd);
            free(command);
            continue;
        }

        if (strcmp(command, "enter_library") == 0) {
            // If the command is enter_library

            // Verify login
            if (loggedIn != 1) {
                printf("You have to login first!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Opening the connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Calling the enter_library function
            enteredLibrary = enter_library(sockfd, cookie, &jwt);

            // Extracting the token
            char *string = calloc(LINELEN, sizeof(char));
            strcpy(string, jwt);

            if (enteredLibrary == 1) {
                char *p = strtok(string, "\n");
                while(p != NULL) {
                    char *found_token = strstr(p, "token");
                    if (found_token) {
                        for (int i = 8; i < strlen(found_token - 2); i++) {
                            token[i - 8] = found_token[i];
                        }
                        token[strlen(found_token) - 10] = '\0';
                    }
                    p = strtok(NULL, " ");
                }
            }

            // Closing the socket and freeing the command space
            printf("\n\n");
            free(string);
            close(sockfd);
            continue;
        }

        if (strcmp(command, "get_books") == 0) {
            // If the command is get_books

            // Verify the login
            if (loggedIn != 1) {
                printf("You have to login first!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Verify if enter_library command was called
            if (enteredLibrary != 1) {
                printf("You must first connect to the library to get books!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Open the socket
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Call the get_books function
            get_books(sockfd, token, cookie);

            // Closing the socket and freeing the command space
            printf("\n\n");
            close(sockfd);
            free(command);
            continue;
        }

        if (strcmp(command, "get_book") == 0) {
            // If the command is get_book

            // Verify login
            if (loggedIn != 1) {
                printf("You have to login first!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Verify if enetered library
            if (enteredLibrary != 1) {
                printf("You must first connect to the library to get a book!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Reading the id
            int id;
            printf("id=");
            scanf("%d", &id);
            scanf("%c", &end_line);

            // Opening the connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Call the get_book function
            get_book(sockfd, id, token);

            // Closing the socket and freeing the command space
            printf("\n\n");
            free(command);
            close(sockfd);
            continue;
        }

        if (strcmp(command, "add_book") == 0) {
            // If the command is add_book

            // Verify login
            if (loggedIn != 1) {
                printf("You have to login first!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Verify if entered the library
            if (enteredLibrary != 1) {
                printf("You must first connect to the library to add a book!\n");
                free(command);
                printf("\n\n");
                continue;
            }

            // Allocating memory for the new book
            struct book *new_book = malloc(sizeof(struct  book));
            new_book->title = malloc(NAME_LEN * sizeof(char));
            new_book->author = malloc(NAME_LEN * sizeof(char));
            new_book->genre = malloc(NAME_LEN * sizeof(char));
            new_book->publisher = malloc(NAME_LEN * sizeof(char));
            new_book->page_count = -1;

            // Reading the fields
            printf("title=");
            fgets(new_book->title, NAME_LEN, stdin);
            new_book->title[strlen(new_book->title) - 1] = '\0';
            if (strlen(new_book->title) == 0) {
                printf("Not a valid title!\n");

                free(command);
                continue;
            }

            printf("author=");
            fgets(new_book->author, NAME_LEN, stdin);
            new_book->author[strlen(new_book->author) - 1] = '\0';
            if (strlen(new_book->author) == 0) {
                printf("Not a valid author name!\n");

                free(command);
                continue;
            }

            printf("genre=");
            fgets(new_book->genre, NAME_LEN, stdin);
            new_book->genre[strlen(new_book->genre) - 1] = '\0';
            if (strlen(new_book->genre) == 0) {
                printf("Not a valid genre!\n");

                free(command);
                continue;
            }

            printf("publisher=");
            fgets(new_book->publisher, NAME_LEN, stdin);
            new_book->publisher[strlen(new_book->publisher) - 1] = '\0';
            if (strlen(new_book->publisher) == 0) {
                printf("Not a valid publisher!\n");

                free(command);
                continue;
            }

            char *page_verifyer = calloc(NAME_LEN, sizeof(char));
            printf("page_count=");
            fgets(page_verifyer, NAME_LEN, stdin);
            page_verifyer[strlen(page_verifyer) - 1] = '\0';

            // Verify if the page count is correct
            int ok = 0;
            for (char i = 'a'; i <= 'z'; i++) {
                if (strchr(page_verifyer, i)) {
                    printf("Not a valid page_count!\n");
                    ok = 1;
                    break;
                }
            }

            if (ok == 1) {
                free(command);
                printf("\n\n");
                continue;
            }

            for (char i = 'A'; i <= 'Z'; i++) {
                if (strchr(page_verifyer, i)) {
                    printf("Not a valid page_count!\n");
                    ok = 1;
                    break;
                }
            }

            if (ok == 1) {
                free(command);
                printf("\n\n");
                continue;
            }

            new_book->page_count = atoi(page_verifyer);
            if(new_book->page_count <= 0) {
                printf("Not a valid page_count!\n");
                free(command);
                printf("\n\n");
                continue;
            }
            
            // Opening the connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Calling the add_book function
            add_book(sockfd, new_book, token, cookie);

            // Closing the socket and freeing the command space
            printf("\n\n");
            free(new_book->title);
            free(new_book->author);
            free(new_book->genre);
            free(new_book->publisher);
            free(command);
            free(new_book);
            close(sockfd);
            continue;
        }

        if (strcmp(command, "delete_book") == 0) {
            // If the command is delete_book

            // Verify if login
            if (loggedIn != 1) {
                printf("You have to login first!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Verify if enetered library
            if (enteredLibrary != 1) {
                printf("You must first connect to the library to delete a book!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            //Reading the id
            int id;
            printf("id=");
            scanf("%d", &id);
            scanf("%c", &end_line);

            // Opening the socket
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Calling the delete_book function
            delete_book(sockfd, id, token);

            // Closing the socket and freeing the memory
            printf("\n\n");
            free(command);
            close(sockfd);
            continue;
        }

        if (strcmp(command, "logout") == 0) {
            // If the command is logout

            // If the command is login
            if (loggedIn != 1) {
                printf("You did not login!\n");
                free(command);

                printf("\n\n");
                continue;
            }

            // Opening the socket connection
            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);

            // Calling the logout function and setting the two verifiers
            // on 0 if the logout was suvccessful or on 1 if it was
            // unsuccessful
            loggedIn = logout(sockfd, cookie);
            enteredLibrary = loggedIn;

            // Freeing the memory and allocating new one for next possible login
            free(cookie);
            free(jwt);
            free(token);
            cookie = calloc(LINELEN, sizeof(char));
            token = calloc(LINELEN, sizeof(char));
            jwt = calloc(LINELEN, sizeof(char));

            // Closing the socket and freeing the command space
            printf("\n\n");
            close(sockfd);
            free(command);
            continue;
        }

        // Printing an unknown command error
        printf("Unknown command!\n");
        printf("Type one of the following commands: \n");
        printf("-- register\n");
        printf("-- login\n");
        printf("-- enter_library\n");
        printf("-- get_books\n");
        printf("-- get_book\n");
        printf("-- add_book\n");
        printf("-- delete_book\n");
        printf("-- logout\n");
        printf("-- exit\n");
        printf("\n");
        free(command);
    }

    return 0;
}