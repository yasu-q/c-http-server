# ifndef HTTP_UTIL_H
# define HTTP_UTIL_H

#include "arraylist.h"

#include <stddef.h>

// =------ typedefs and structs ------=

typedef enum {
    HTTP_UTIL_OK = 1,
    HTTP_UTIL_BAD_INPUT = -1,
    HTTP_UTIL_INVALID_FORMAT = -2,
    HTTP_UTIL_MEMALLOC_FAIL = -3,
    HTTP_UTIL_LINE_NOT_FOUND = -4,
    HTTP_UTIL_DUPLICATE_HEADER = -5,
    HTTP_UTIL_MISSING_CONTENT_LENGTH = -6,
    HTTP_UTIL_INVALID_CONTENT_LENGTH = -7,
    HTTP_UTIL_STRFUN_FAIL = -8,
    HTTP_UTIL_MISCFAIL = -9,
} Http_Error;

typedef enum {
    HTTP_OK_200 = 200,
    HTTP_METNOALLOWED_405 = 405,
    HTTP_NOTFOUND_404 = 404,
    HTTP_SERVERR_500,
} Http_Status_Code;

// Request

typedef struct {
    char *method;
    char *request_target;
    char *http_version;
} Request_Line;

typedef struct {
    char *key;
    char *value;
} Header;

typedef struct {
    Request_Line request_line;
    ArrayList *headers;
    char *body;
    size_t body_len;
} Http_Request;

// Response

typedef struct {
    char *http_version;
    Http_Status_Code status_code;
    char *reason_phrase;
} Status_Line;

typedef struct {
    Status_Line status_line;
    ArrayList *headers;
    char *body;
    size_t body_len;
} Http_Response;

// =------ functions ------=

// General functions

/*
*   Get entire substring before other substring
*
*   substr_prev("hello", "lo") = "hel"
*   
*   This will malloc the returned stringa i.e it's different
*   than like a reverse strstr(). Kinda like strtok().
*/
char *substr_prev(const char *str, const char* brkpt);

/*
*   Case insensitive string comparison. Will segfault if strings are bad
*/
int caseinsensitive_strcmp(char *str1, char *str2);

// Request line

/*
*   Parse request line
*
*   This expects a *complete* http request
*
*   Will fill in req_line's http request line fields with 
*   memory allocated for them. Will also make request point to
*   the next bit of the request that should be scanned i.e. the 2nd
*   line. It will not include the request line's \r\n and will
*   start directly on the first character of the next line
*   
*   The passed req_line should be allocated.
*
*   Returns 0 on success, -1 on error
*/
int parse_request_line (Request_Line *req_line, char **request);

/*
*   Print a Request_Line
*/
void print_request_line(Request_Line *req_line);

// Headers

/*
*   Initialize the headers array list
*/
void init_headers(ArrayList **headers);

/*
*   Parse a single header line. 
*   
*   Fills the header with contents. The contents will
*   *be malloc'd* but will not malloc header itself.
*   expects declared header.   
*
*   Returns -1 on fail 1 on success
*/
int parse_header(Header *header, char **request);

/*
*   Parse the header fields.
*
*   This expects an initialized header array list. Call init_headers()
*   on the array list before passing it into this function.
*
*   Will fill the passed headers array list with headers
*   and move the processing line along by moddifying where
*   requests poitns to in the overall request. At this point, the
*   request will be either empty of just contain the body.
*
*   This will *malloc the headers* in the arraylist.
*
*   Returns 0 on success, -1 on error
*/
int parse_headers(ArrayList *headers, char **request);

/*
*   Print headers
*/
void print_header(Header *header);

/*
*   Print headers    
*/
void print_headers(ArrayList *headers);

// Body

/*
*   Returns 0 if two passed headers have matching
*   header names NON-CASE SENSITIVE
*
*   content-length = CONTENT-LENGTH 
*
*   -1 on error. 1 if they are not equal.
*/
int header_key_comparator(void *h1, void *h2);

/*
*   Fills in the passed body with the body of the http request in
*   request based on the information in headers. 
*
*   Will allocate space for body, though will not allocate space for
*   pointer itself. Will also make request point to an empty string
*   indicating that the request parsing is done. If request is not empty
*   then there is a chance the content length was incorrect/did not match
*   the size of the body. The returned body string is *NOT* null terminated.
*
*   Returns -1 on fail 
*/
int parse_body(char **body, char **request, ArrayList *headers, size_t *body_len);

/*
*   Print the body contents. Needs a reference size since bodies are not generally
*   NULL terminated 
*/
void print_body(char *body, size_t body_len);

/*
*   Fill in the passed request. Pipe it through the previous functions
*   to create a request. Must not be passed as NULL i.e. alloc'd before
*   passing
*
*   Will make request based on data's content, which should be a *complete*
*   HTTP 1.1 which is *NULL terminated*. data does not have to be a string, but
*   there should be a \0 signalling its end
*/
int new_request(Http_Request *request, char *data);

/*
*   Print the contents of an HTTP request
*/
void print_request(Http_Request *request);

// Response

/*
*   Helper for new_response()
*/
void append_content_type(Http_Response *response, const char *filepath);

/*
*   Make a new http response based off a request
*/
int new_response(Http_Response *response, Http_Request *request, const char *dir);

/*
*   Response length. helper to response_mssg()
*/
int response_len(size_t *len, Http_Response *response);

/*
*   Make a response message based off a response
*/
int response_mssg(char **buf, size_t *response_len, Http_Response *response);

/*
*   print req line
*/
void print_status_line(Status_Line *status_line);

/*
*   print response structs
*/
void print_response(Http_Response *response);

/*
*   is file ? 1 : 0
*/
int is_file(const char *path);

/*
*   these functions fill in the response with the appropriate status line, body, and headers
*/
int method_not_allowed(Http_Response *response);
int internal_server_error(Http_Response *response);
int forbidden_request(Http_Response *response);
int not_found(Http_Response *response);
int add_header_content_length(Http_Response *response, int len);

// destructors

// for all of these functions, only the fields will be freed
// the pointer to the struct itself is not freed

void free_request_line(Request_Line *request_line);
void free_header(void *header); // ArrayList expects the free function to have type void (*func)(void *)
void free_headers(ArrayList *headers);
void free_request(Http_Request *request);
void free_response(Http_Response *response);

#endif