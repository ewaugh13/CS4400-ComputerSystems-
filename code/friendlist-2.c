/*
 * friendlist.c - [Starting code for] a web-based friend-graph manager.
 *
 * Based on:
 *  tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *      GET method to serve static and dynamic content.
 *   Tiny Web server
 *   Dave O'Hallaron
 *   Carnegie Mellon University
 */
#include "csapp.h"
#include "dictionary.h"
#include "more_string.h"

static void doit(int fd);
void *go_doit(void *connfdp);
static dictionary_t *read_requesthdrs(rio_t *rp);
static void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *d);
static void clienterror(int fd, char *cause, char *errnum, 
                        char *shortmsg, char *longmsg);
static void print_stringdictionary(dictionary_t *d);
static void get_friends(int fd, dictionary_t *query);
static void befriend(int fd, dictionary_t *query);
static void unfriend(int fd, dictionary_t *query);
static void introduce(int fd, dictionary_t *query);

dictionary_t *users;
pthread_mutex_t lock;
pthread_mutex_t introduce_lock;

int main(int argc, char **argv) 
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (pthread_mutex_init(&lock, NULL) != 0)
  {
    fprintf(stderr,"mutex init failed\n");
    return 1;
  }
  if (pthread_mutex_init(&introduce_lock, NULL) != 0)
  {
    fprintf(stderr,"mutex init failed\n");
    return 1;
  }

  users = make_dictionary(COMPARE_CASE_SENS, free);

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  /* Don't kill the server if there's an error, because
     we want to survive errors due to a client. But we
     do want to report errors. */
  exit_on_error(0);

  /* Also, don't stop on broken connections: */
  Signal(SIGPIPE, SIG_IGN);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    if (connfd >= 0) {
      Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
                  port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      {
        int *connfdp;
        pthread_t th;
        connfdp = malloc(sizeof(int));
        *connfdp = connfd;
        Pthread_create(&th, NULL, go_doit, connfdp);
        Pthread_detach(th);
      }
    }
  }
}

void *go_doit(void *connfdp)
{
  int connfd = *(int *)connfdp;
  free(connfdp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd) 
{
  char buf[MAXLINE], *method, *uri, *version;
  rio_t rio;
  dictionary_t *headers, *query;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (Rio_readlineb(&rio, buf, MAXLINE) <= 0)
    return;
  printf("%s", buf);
  
  if (!parse_request_line(buf, &method, &uri, &version)) {
    clienterror(fd, method, "400", "Bad Request",
                "Friendlist did not recognize the request");
  } else {
    if (strcasecmp(version, "HTTP/1.0")
        && strcasecmp(version, "HTTP/1.1")) {
      clienterror(fd, version, "501", "Not Implemented",
                  "Friendlist does not implement that version");
    } else if (strcasecmp(method, "GET")
               && strcasecmp(method, "POST")) {
      clienterror(fd, method, "501", "Not Implemented",
                  "Friendlist does not implement that method");
    } else {
      headers = read_requesthdrs(&rio);

      /* Parse all query arguments into a dictionary */
      query = make_dictionary(COMPARE_CASE_SENS, free);
      parse_uriquery(uri, query);
      if (!strcasecmp(method, "POST"))
        read_postquery(&rio, headers, query);

      /* For debugging, print the dictionary */
      print_stringdictionary(query);

      /* You'll want to handle different queries here,
         but the intial implementation always returns
         nothing: */
      if (starts_with("/friends", uri))
      {
        get_friends(fd, query);
      }
      else if (starts_with("/befriend", uri))
      {
        befriend(fd, query);
      }
      else if (starts_with("/unfriend", uri))
      {
        unfriend(fd, query);
      }
      else if (starts_with("/introduce", uri))
      {
        introduce(fd, query);
      }

      /* Clean up */
      free_dictionary(query);
      free_dictionary(headers);
    }

    /* Clean up status line */
    free(method);
    free(uri);
    free(version);
  }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
dictionary_t *read_requesthdrs(rio_t *rp) 
{
  char buf[MAXLINE];
  dictionary_t *d = make_dictionary(COMPARE_CASE_INSENS, free);

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    parse_header_line(buf, d);
  }
  
  return d;
}

void read_postquery(rio_t *rp, dictionary_t *headers, dictionary_t *dest)
{
  char *len_str, *type, *buffer;
  int len;
  
  len_str = dictionary_get(headers, "Content-Length");
  len = (len_str ? atoi(len_str) : 0);

  type = dictionary_get(headers, "Content-Type");
  
  buffer = malloc(len+1);
  Rio_readnb(rp, buffer, len);
  buffer[len] = 0;

  if (!strcasecmp(type, "application/x-www-form-urlencoded")) {
    parse_query(buffer, dest);
  }

  free(buffer);
}

static char *ok_header(size_t len, const char *content_type) {
  char *len_str, *header;
  
  header = append_strings("HTTP/1.0 200 OK\r\n",
                          "Server: Friendlist Web Server\r\n",
                          "Connection: close\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n",
                          "Content-type: ", content_type, "\r\n\r\n",
                          NULL);
  free(len_str);

  return header;
}

/*
 * get_friends - gets all the friends of a selected user from the query 
 */
static void get_friends(int fd, dictionary_t *query)
{
  pthread_mutex_lock(&lock);
  size_t len;
  char *body, *header, *user, *friends;
  friends = "";
  
  if(dictionary_count(query) != 1)
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide only 1 user");
    return;
  }

  user = dictionary_get(query, "user");
  if (!user) 
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide a user");
    return;
  }
  dictionary_t *friends_of_user = (dictionary_t*)dictionary_get(users, user);
  if(friends_of_user != NULL)
  {
    const char **friends_list = dictionary_keys(friends_of_user);
    friends = join_strings((const char * const *)friends_list, '\n');
    free(friends_list);
  }

  body = friends;
  
  len = strlen(body);
  
  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);
  
  free(header);
  
  /* Send response body to client */
  Rio_writen(fd, body, len);
  if(len > 0)
  {
    free(body);
  }
  pthread_mutex_unlock(&lock);
}

/*
 * befriend - befriends two users
 */
 static void befriend(int fd, dictionary_t *query)
 {
   pthread_mutex_lock(&lock);
   size_t len;
   char *body, *header, *user, *friends, *all_friends;
   all_friends = "";
   
   if(dictionary_count(query) != 2)
   {
     clienterror(fd, "?", "400", "Bad Request","Please provide only the user and the friends");
     return;
   }
 
   user = dictionary_get(query, "user");
   friends = dictionary_get(query, "friends");
   if (!user || !friends) 
   {
     clienterror(fd, "?", "400", "Bad Request","Please provide a user and the friends");
     return;
   }
   char **friends_list = split_string(friends, '\n');
   dictionary_t *friends_of_user = (dictionary_t*)dictionary_get(users, user);
   if(friends_of_user == NULL)
   {
     friends_of_user = make_dictionary(COMPARE_CASE_SENS, free);
     dictionary_set(users, user, friends_of_user);
   }
   int i;
   for (i = 0; friends_list[i] != NULL; i++)
   {
     dictionary_set(friends_of_user, friends_list[i], NULL);
     dictionary_t *the_friend_friends = (dictionary_t*)dictionary_get(users, friends_list[i]);
     if(the_friend_friends == NULL)
     {
       the_friend_friends = make_dictionary(COMPARE_CASE_SENS, free);
       dictionary_set(users, friends_list[i], the_friend_friends);
     }
     dictionary_set(the_friend_friends, user, NULL);
   }
   const char **all_friends_list = dictionary_keys(friends_of_user);
   all_friends = join_strings((const char * const *)all_friends_list, '\n');
 
   body = all_friends;
   
   len = strlen(body);
   
   /* Send response headers to client */
   header = ok_header(len, "text/html; charset=utf-8");
   Rio_writen(fd, header, strlen(header));
   printf("Response headers:\n");
   printf("%s", header);
   
   free(header);
   
   /* Send response body to client */
   Rio_writen(fd, body, len);
   free(body);
   pthread_mutex_unlock(&lock);
 }

 /*
 * unfriend - unfriends two users
 */
 static void unfriend(int fd, dictionary_t *query)
 {
  pthread_mutex_lock(&lock);
  size_t len;
  char *body, *header, *user, *friends, *all_friends;
  all_friends = "";
  
  if(dictionary_count(query) != 2)
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide only the user and the friends");
    return;
  }

  user = dictionary_get(query, "user");
  friends = dictionary_get(query, "friends");
  if (!user || !friends) 
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide a user and the friends");
    return;
  }
  char **friends_list = split_string(friends, '\n');
  dictionary_t *friends_of_user = (dictionary_t*)dictionary_get(users, user);
  if(friends_of_user != NULL)
  {
    int i;
    for (i = 0; friends_list[i]; i++)
    {
      dictionary_remove(friends_of_user, friends_list[i]);
      dictionary_t *friend_removed_friends = (dictionary_t*)dictionary_get(users, friends_list[i]);
      dictionary_remove(friend_removed_friends, user);
    }
  }

  const char **all_friends_list = dictionary_keys(friends_of_user);
  all_friends = join_strings((const char * const *)all_friends_list, '\n');

  body = all_friends;
  
  len = strlen(body);
  
  /* Send response headers to client */
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  printf("Response headers:\n");
  printf("%s", header);
  
  free(header);
  
  /* Send response body to client */
  Rio_writen(fd, body, len);
  free(body);
  free(friends_list);
  free(all_friends_list);
  pthread_mutex_unlock(&lock);
 }

 /*
 * introduce - introduce user and friend plus friends of friend on another server
 */
 static void introduce(int fd, dictionary_t *query)
 {
  pthread_mutex_lock(&introduce_lock);
  size_t len;
  char *header, *user, *friend, *connection_host, *connection_port;
  
  if(dictionary_count(query) != 4)
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide the user and the friend plus the host and the port (4 args)");
    return;
  }

  user = dictionary_get(query, "user");
  friend = dictionary_get(query, "friend");
  connection_host = dictionary_get(query, "host");
  connection_port = dictionary_get(query, "port");
  if (!user || !friend || !connection_host || !connection_port) 
  {
    clienterror(fd, "?", "400", "Bad Request","Please provide the user and the friend plus the host and the port");
    return;
  }

  struct addrinfo hints, *addrs;
  int s;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET; /* Request IPv4 */
  hints.ai_socktype = SOCK_STREAM; /* Accept TCP connections */
  hints.ai_flags = AI_PASSIVE; /* ... on any IP address */
  Getaddrinfo(connection_host, connection_port, &hints, &addrs);

  s = Socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
  Connect(s, addrs->ai_addr, addrs->ai_addrlen);
  Freeaddrinfo(addrs);
  //make my own query
  header = append_strings("GET /friends?user=", friend, " HTTP/1.1\r\n",
  "Host: ", connection_host, ":", connection_port, "\r\n",
  "User-Agent: friend_list\r\n"
  "Content-Length: 0\r\n\r\n");

  rio_writen(s, header, strlen(header));

  rio_t rio;
  size_t bytes_read = 0;
  char buf[MAXLINE];
  Rio_readinitb(&rio, s);

  dictionary_t *returned_header_info = read_requesthdrs(&rio);
  char *string_count = (char*)dictionary_get(returned_header_info, "Content-length");
  size_t byte_count = atoi(string_count);

  dictionary_t *new_befriend_query = make_dictionary(COMPARE_CASE_SENS, free);
  dictionary_set(new_befriend_query,"user",user);
  dictionary_t *friends_recieved = make_dictionary(COMPARE_CASE_SENS, free);
  while(bytes_read < byte_count)
  {
    bytes_read += rio_readlineb(&rio, buf, MAXLINE);
    char *friend_to_introduce = buf;
    friend_to_introduce = split_string((const char *)friend_to_introduce, '\n')[0];
    if(!(strcmp(friend_to_introduce, user) == 0))
    {
      dictionary_set(friends_recieved,friend_to_introduce,NULL);
    }
  }
  dictionary_set(friends_recieved,friend,NULL);
  const char **friends_recieved_list = dictionary_keys(friends_recieved);
  char *friends = join_strings((const char * const *)friends_recieved_list, '\n');
  dictionary_set(new_befriend_query,"friends",friends);

  int *new_fdp;
  new_fdp = malloc(sizeof(int));
  befriend(*new_fdp,new_befriend_query);
  Close(*new_fdp);
  free(new_fdp);

  Shutdown(s, SHUT_WR);

  len = 0;  
  header = ok_header(len, "text/html; charset=utf-8");
  Rio_writen(fd, header, strlen(header));
  
  free_dictionary(returned_header_info);
  free_dictionary(friends_recieved);
  free(friends_recieved_list);
  free(header);
  free(friends);
  pthread_mutex_unlock(&introduce_lock);
 }

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  size_t len;
  char *header, *body, *len_str;

  body = append_strings("<html><title>Friendlist Error</title>",
                        "<body bgcolor=""ffffff"">\r\n",
                        errnum, " ", shortmsg,
                        "<p>", longmsg, ": ", cause,
                        "<hr><em>Friendlist Server</em>\r\n",
                        NULL);
  len = strlen(body);

  /* Print the HTTP response */
  header = append_strings("HTTP/1.0 ", errnum, " ", shortmsg, "\r\n",
                          "Content-type: text/html; charset=utf-8\r\n",
                          "Content-length: ", len_str = to_string(len), "\r\n\r\n",
                          NULL);
  free(len_str);
  
  Rio_writen(fd, header, strlen(header));
  Rio_writen(fd, body, len);

  free(header);
  free(body);
}

static void print_stringdictionary(dictionary_t *d)
{
  int i, count;

  count = dictionary_count(d);
  for (i = 0; i < count; i++) {
    printf("%s=%s\n",
           dictionary_key(d, i),
           (const char *)dictionary_value(d, i));
  }
  printf("\n");
}

