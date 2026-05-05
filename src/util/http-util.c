#include "http-util.h"
#include "arraylist.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <limits.h> // Provides PATH_MAX
#include <fcntl.h>
#include <unistd.h>
// Testing
#include <stdio.h>

// registered nurse line end
#define RN "\r\n"
#define RN_LEN (sizeof(RN) - 1) // 1 char = 1 byte; - 1 for \0
#define HTTP_VER "HTTP/1.1" // HTTP version this file is built for
#define HTTP_RPHRASE_OK "OK"
#define STATUS_CODE_LEN 3

/* 
*   Some notes:
*   - When creating structs, ensure that *every* char* field is malloc'd
*     since the destructor functions will free these fields
*       - The only exception to this rule is Status_Line, whose string fields
*         are static strings
*/

char *substr_prev(const char *str, const char* brkpt) {
    const char *lineend = NULL;
    int sublen = 0;

    if (str == NULL || brkpt == NULL) {
        fprintf(stderr, "substr_prev: str or brkpt was NULL\n");
        return NULL;
    }

    lineend = strstr(str, brkpt);

    if (lineend == NULL) { // If no valid substring is found
        return NULL;
    }

    sublen = lineend - str;

    // Return string
    char *ret = malloc(sublen + 1);
    if (ret == NULL) {
        fprintf(stderr, "substr_prev: malloc fail\n");
        return NULL;
    }

    memcpy(ret, str, sublen); 
    ret[sublen] = '\0';

    return ret;
}

int caseinsensitive_strcmp(char *str1, char *str2) {
    int char1 = ' ';
    int char2 = ' ';

    while (*str1 != '\0' && *str2 != '\0') {
        char1 = tolower(*str1);
        char2 = tolower(*str2);

        if (char1 != char2) {
            // Strings don't match case insentive
            break;
        }

        str1++;
        str2++;
    }

    return tolower(*str1) - tolower(*str2);
}

int parse_request_line (Request_Line *req_line, char **request) {
    char *start_line = NULL; // FREE THIS AT THE END
    char *after_start = NULL;
    // Individual components
    char *met = NULL;
    char *req_tar = NULL;
    char *http_v = NULL;

    if (req_line == NULL || request == NULL) {
        fprintf(stderr, "parse_request_line: req_line or request NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // =---- Get start line ----=

    // Start line. From start to RN = \r\n
    start_line = substr_prev(*request, RN); // Remember that this mallocs start_line
    if (start_line == NULL) {
        fprintf(stderr, "parse_request_line: unable to find \\r\\n in start line\n");
        return HTTP_UTIL_INVALID_FORMAT;
    }
    // Line after start line. Jump the \r and \n characters
    after_start = strstr(*request, RN);
    if (after_start == NULL) {
        fprintf(stderr, "parse_request_line: strstr() failed\n");
        return HTTP_UTIL_STRFUN_FAIL;
    }
    after_start = after_start + RN_LEN;

    // =---- Extract information from start line ----=
    
    met = strtok(start_line, " ");
    req_tar = strtok(NULL, " ");
    http_v = strtok(NULL, " "); 
    if (met == NULL || req_tar == NULL || http_v == NULL) {
        fprintf(stderr, "parse_request_line: invalid start line\n");
        free(start_line);
        return HTTP_UTIL_INVALID_FORMAT;
    }

    // =---- Setup req_line ----=

    // Allocate new memory to req_line's 
    // fields so we can free start_line 
    req_line->method = strdup(met);
    if (!req_line->method) {
        fprintf(stderr, "parse_request_line: strdup() fail for method\n");
        free(start_line);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }
    req_line->request_target = strdup(req_tar);
    if (!req_line->request_target) {
        fprintf(stderr, "parse_request_line: strdup() fail for request_target\n");
        free(start_line);
        free(req_line->method);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }
    req_line->http_version = strdup(http_v);
    if (!req_line->http_version) {
        fprintf(stderr, "parse_request_line: strdup() fail for http_version\n");
        free(start_line);
        free(req_line->method);
        free(req_line->request_target);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    // Make request point to the rest signaling 
    // that the start line has been processed
    *request = after_start;
    
    free(start_line);

    // TODO: validate whether this was a valid HTTP 1.1 request line

    return HTTP_UTIL_OK;
}

void print_request_line(Request_Line *req_line) {
    if (req_line == NULL) {
        fprintf(stderr, "print_request_line: req_line was NULL\n");
    } else {
        char *met = req_line->method;
        char *req_tar = req_line->request_target;
        char *http_v = req_line->http_version;
    
        printf("request line:\n\tmethod: \"%s\" | req tar: \"%s\" | http v: \"%s\"\n", 
                met, req_tar, http_v);
    }
}

/*
*   Just assume that it's a header line for now. The
*   parse_headers() function should dertermine when
*   the headers end
*/
int parse_header(Header *header, char **request) {
    char *header_line = NULL; // FREE THIS AT THE END
    char *after_line = NULL; // Hold the next line to reset request to
    // Header components
    char *ke = NULL;
    char *val = NULL;

    if (header == NULL || request == NULL) {
        fprintf(stderr, "parse_header: header or request was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // =---- Get header line ----=

    header_line = substr_prev(*request, RN); // This mallocs. Free later
    if (header_line == NULL) {
        fprintf(stderr, "parse_header: unable to find \\r\\n in header line\n");
        free(header_line);
        return HTTP_UTIL_INVALID_FORMAT;
    }
    // Line after start line. Jump the \r and \n characters
    after_line = strstr(*request, RN);
    if (after_line == NULL) {
        fprintf(stderr, "parse_header: strstr() failed\n");
        free(header_line);
        return HTTP_UTIL_STRFUN_FAIL;
    }
    after_line = after_line + RN_LEN; // Skip \r\n

    // =---- Extract information ----=

    char *split = strchr(header_line, ':');
    if (split == NULL) {
        fprintf(stderr, "parse_header: strchr() fail\n");
        return HTTP_UTIL_STRFUN_FAIL;
    }
    if (split != NULL) {
        *split = '\0';

        ke = header_line; // Go to first :
        val = split + 1; // After first split
        while (*val == ' ') { // Skip whitespace
            val++;
        }
    }   

    header->key = strdup(ke);
    if (!header->key) {
        fprintf(stderr, "parse_header: strdup failed for key\n");
        free(header_line);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }
    header->value = strdup(val);
    if (!header->value) {
        fprintf(stderr, "parse_header: strdup failed for value\n");
        free(header->key);
        free(header_line);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    *request = after_line;

    free(header_line);

    // TODO: validate whether the header line is actually valid HTTP 1.1
    // I'm just going to to assume that all messages passed are valid
    // for now.

    return HTTP_UTIL_OK;
}

void init_headers(ArrayList **headers) {
    *headers = ArrayList_new(free_header); 
}

// TODO: CHECK DUPLICATES!!!!
// SOME HEADER NAMES CANNOT HAVE DUPLICATES e.g. CONTENT-LENGTH
// IMPLEMENT PROPER ERROR CODE INDICATING THIS
int parse_headers(ArrayList *headers, char **request) {
    char *cur_line = NULL; // Current line
    Header *new_header = NULL;

    if (headers == NULL || request == NULL) {
        fprintf(stderr, "parse_headers: headers or request was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // =---- Parse headers until cur line is empty ----=
 
    // And by empty I mean is just a \r\n
    cur_line = *request; // Set to first line

    // Check if first 4 chars of cur_line are exactly "\r\n\r\n"
    while (cur_line[0] != '\r' && cur_line[1] != '\n' &&
           cur_line[2] != '\r' && cur_line[3] != '\n') {
        // // debugging. print bytes
        // for (size_t i = 0; cur_line[i] != '\0'; i++) {
        //     printf("%02X ", (unsigned char)cur_line[i]);
        // }
        // printf("bef\n");

        new_header = malloc(sizeof(Header));
        if (new_header == NULL) {
            fprintf(stderr, "parse_headers: malloc fail\n");
            return HTTP_UTIL_MEMALLOC_FAIL;
        }

        if (parse_header(new_header, &cur_line) != HTTP_UTIL_OK) {
            fprintf(stderr, "parse_headers: parse_header fail\n");
            free(new_header);
            return HTTP_UTIL_MISCFAIL;
        }

        if (ArrayList_insert(headers, new_header) == 0) {
            fprintf(stderr, "parse_headers: fail to insert into ArrayList\n");
            free(new_header);
            return HTTP_UTIL_MISCFAIL;
        }
    }

    // Skip the /r/n
    *request = cur_line + RN_LEN;

    // At this point, the *request will be either
    // empty or just contain the body.

    return HTTP_UTIL_OK;
}

void print_header(Header *header) {
    if (header == NULL) {
        fprintf(stderr, "print_header: header was NULL\n");
    } else {
        printf("header:\n\tkey: \"%s\" | value: \"%s\"\n", 
                header->key, header->value);
    }
}

void print_headers(ArrayList *headers) {
    if (headers == NULL) {
        fprintf(stderr, "print_headers: headers was NULL\n");
    } else {
        for (int i = 0; i < headers->size; i++) {
            print_header((Header *)headers->contents[i]);
        }
    }
}

int header_key_comparator(void *h1, void *h2) {
    if (h1 == NULL || h2 == NULL) {
        fprintf(stderr, "header_key_comparator: h1 or h2 was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    Header *header1 = (Header *)h1;
    Header *header2 = (Header *)h2;

    if (header1->key == NULL || header2->key == NULL) {
        fprintf(stderr, "header_key_comparator: h1 or h2 key was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    if (caseinsensitive_strcmp(header1->key, header2->key) != 0) {
        return 1;
    }

    return 0;
}

int parse_body(char **body, char **request, ArrayList *headers, size_t *body_len) {
    long int content_len = 0;
    char *len = NULL;
    Header dummy_header;
    Header *content_len_header;

    if (body == NULL || request == NULL|| headers == NULL) {
        fprintf(stderr, "parse_body: body, request, or headers are NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // =---- Extract the content length from headers ----=

    dummy_header.key = "content-length";
    content_len_header = ArrayList_getel(
        headers,
        (void *)&dummy_header,
        header_key_comparator
    );
    if (content_len_header == NULL) {
        // It's ok if there is no content length header
        // but if this is the case we will assume there
        // is simply no body
        return HTTP_UTIL_OK;
    }

    len = content_len_header->value;
    content_len = strtol(len, NULL, 10);
    if (content_len == 0) { // If this happens, idk
        *body = NULL;
        return HTTP_UTIL_OK;
    }

    // =---- Copy content onto passed body string ----=

    *body = malloc(content_len); // Allocate content_len bytes
    if (*body == NULL) {
        fprintf(stderr, "parse_body: malloc fail\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }
   
    memcpy(*body, *request, content_len);
    *request += content_len; // Advance request. Will be empty
    *body_len = content_len;

    return HTTP_UTIL_OK;
}

void print_body(char *body, size_t body_len) {
    if (body == NULL) {
        printf("body contents:\n\tempty body\n");
    } else {
        printf("body contents:\n\t\"%.*s\"\n", (int)body_len, body);
    }
}

int new_request(Http_Request *request, char *data) {
    Request_Line *request_line = NULL; // Alloc this later
    ArrayList *headers = NULL; // Initialize later
    char *body = NULL;
    char *request_ptr = NULL;

    int status = HTTP_UTIL_OK;

    if (request == NULL || data == NULL) {
        fprintf(stderr, "new_request: request or data was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    request_ptr = data; // Start of data

    // Make request line
    request_line = malloc(sizeof(*request_line));
    if (request_line == NULL) {
        fprintf(stderr, "new_request: malloc fail\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }
    status = parse_request_line(request_line, &request_ptr);
    if (status != HTTP_UTIL_OK) {
        fprintf(stderr, "new_request: fail to parse request line\n");
        free(request_line);
        return HTTP_UTIL_MISCFAIL;
    }

    // Headers
    init_headers(&headers);
    if (headers == NULL) {
        fprintf(stderr, "new_request: fail to initialize headers\n");
        free(request_line);
        return HTTP_UTIL_MISCFAIL;
    }
    status = parse_headers(headers, &request_ptr);
    if (status != HTTP_UTIL_OK) {
        fprintf(stderr, "new_request: fail to parse headers\n");
        free(request_line);
        ArrayList_free(headers);
        return HTTP_UTIL_MISCFAIL;
    }

    // Body
    status = parse_body(&body, &request_ptr, headers, &request->body_len);
    if (status != HTTP_UTIL_OK) {
        fprintf(stderr, "new_request: fail to parse body \n");
        free(request_line);
        ArrayList_free(headers);
        return HTTP_UTIL_MISCFAIL;
    }

    // Set everything
    request->request_line = *request_line; // Struct copy
    free(request_line); // Free the temporary allocation; fields are now owned by request
    request->headers = headers;
    request->body = body; // Remember, this *may* be NULL

    return status;
}

void print_request(Http_Request *request) {
    if (request == NULL) {
        fprintf(stderr, "print_request: request was NULL\n");
    } else {
        print_request_line(&request->request_line);        
        print_headers(request->headers);
        print_body(request->body, request->body_len);
    }
}

// Response

void append_content_type(Http_Response *response, const char *filepath) {
    Header *content_type = malloc(sizeof(Header));

    if (content_type == NULL) {
        fprintf(stderr, "append_content_type: content_type was NULL\n");
        return;
    } 

    // Get last .
    char *ext = strrchr(filepath, '.');
    if (ext == NULL) {
        content_type->value = strdup("application/octet-stream");
    } else if (strcmp(ext, ".html") == 0) {
        content_type->value = strdup("text/html");
    } else if (strcmp(ext, ".css") == 0) {
        content_type->value = strdup("text/css");
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        content_type->value = strdup("image/jpeg");
    } else if (strcmp(ext, ".png") == 0) {
        content_type->value = strdup("image/png");
    } else if (strcmp(ext, ".js") == 0) {
        content_type->value = strdup("text/javascript");
    } else if (strcmp(ext, ".mp4") == 0) {
        content_type->value = strdup("video/mp4");
    } else {
        // default to binary data?
        content_type->value = strdup("application/octet-stream");
    }

    content_type->key = strdup("Content-Type");
    if (response->headers != NULL) {
       ArrayList_insert(response->headers, content_type);
    }
}

// maybe works?
// Note that this *only* handles GET requests. 
int new_response(Http_Response *response, Http_Request *request, const char *dir) {
    char filepath[PATH_MAX];
    char req_real_path[PATH_MAX];
    char *real_path = NULL;
    int fd = 0;
    struct stat sta;
    size_t bytes_read = 0;

    if (response == NULL || request == NULL || dir == NULL) {
        fprintf(stderr, "new_response: response request or dir was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // We only handled GET requests
    if (caseinsensitive_strcmp(request->request_line.method, "GET") != 0) {
        method_not_allowed(response);
        return HTTP_UTIL_OK;
    }
    // Convert request path into absolute path
    if (realpath(dir, req_real_path) == NULL) {
        fprintf(stderr, "new_response: realpath() fail\n");
        internal_server_error(response);

        return HTTP_UTIL_MISCFAIL;
    }

    // Build file path form request + passed directory
    // If the request is "/", default to index.html
    if (strcmp(request->request_line.request_target, "/") == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", dir);
    } else {
        snprintf(filepath, sizeof(filepath), "%s%s", dir, request->request_line.request_target);
    }
    real_path = realpath(filepath, NULL);

    // Ensure path request is safe? Right now this just gets
    // in the way of 404
    // // Safe path to traverse
    // if (real_path == NULL || strncmp(real_path, req_real_path, strlen(req_real_path)) != 0) {
    //     free(real_path);
    //     forbidden_request(response);
    //     return HTTP_UTIL_OK;
    // }
    
    // Open the file
    // printf("path requested: %s\n", real_path); // Debug
    fd = open(real_path, O_RDONLY);
    if (fd == -1) { 
        free(real_path);
        not_found(response);
        return HTTP_UTIL_OK;
    }
    if (is_file(real_path) == 0) { // 404 wooo
        free(real_path);
        not_found(response);
        return HTTP_UTIL_OK;
    }

    // File size
    if (fstat(fd, &sta) == -1) {
        fprintf(stderr, "new_response: fstat() fail\n");
        close(fd);
        free(real_path);
        internal_server_error(response);
        return HTTP_UTIL_OK;
    }

    // Read into the body
    response->body = malloc(sta.st_size);
    if (response->body == NULL) {
        fprintf(stderr, "new_response: malloc() fail\n");
        close(fd);
        free(real_path);
        internal_server_error(response);

        return HTTP_UTIL_MEMALLOC_FAIL;
    } 

    bytes_read = read(fd, response->body, sta.st_size);
    if (bytes_read != sta.st_size) {
        fprintf(stderr, "new_response: fail to read()\n");
        free(response->body);
        close(fd);
        free(real_path);
        internal_server_error(response);
        
        return HTTP_UTIL_MISCFAIL;
    }
    
    // Create response
    response->status_line.http_version = HTTP_VER;
    response->status_line.status_code = HTTP_OK_200;
    response->status_line.reason_phrase = "OK";
    response->body_len = sta.st_size;
    // Headers
    init_headers(&response->headers);
    add_header_content_length(response, (int)sta.st_size);
    append_content_type(response, real_path);

    // Cleanup
    close(fd);
    free(real_path);

    return HTTP_UTIL_OK;
}

int response_len(size_t *len, Http_Response *response) { 
    // Assume string functions just work for now
    // And all others functions work
    size_t rep_len = 0;
    Header *p = NULL;
    ArrayList *headers = NULL;

    if (len == NULL || response == NULL) {
        fprintf(stderr, "response_len: len or response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    // Figure out the required size of the response

    // Get the length of the status line
    rep_len += strlen(response->status_line.http_version);
    rep_len += 1; // Whitespace
    rep_len += STATUS_CODE_LEN; // 3
    rep_len += 1; // Whitespace
    rep_len += strlen(response->status_line.reason_phrase);
    rep_len += RN_LEN; // The additional \r\n at the end of the line

    // Get lenght of headers
    headers = response->headers;
    if (headers != NULL) {
        for (uint32_t i = 0; i < headers->size; i++) {
            p = ArrayList_get(headers, i); // Extract header
            rep_len += strlen(p->key);
            rep_len += strlen(p->value);
            rep_len += 2; // ": " between key and value
            rep_len += RN_LEN; // \r\n at the end of the line
        }
    }
    rep_len += RN_LEN; // Blank \r\n line indicating headers done

    // Get length of body content
    rep_len += response->body_len;

    // We are done
    *len = rep_len;

    return HTTP_UTIL_OK;
}

/*
*   Just assume valid http response for now i.e. non null stuff
*/
int response_mssg(char **buf, size_t *ret_len, Http_Response *response) {
    size_t len = 0;
    char *cur = NULL; 
    Header *cur_header = NULL;

    if (buf == NULL || ret_len == NULL || response == NULL) {
        fprintf(stderr, "response_mssg: buf request_len or responose was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    response_len(&len, response);
    char *ret_buf = malloc(len); // Will not allocate space for an extra \0 at the end
    cur = ret_buf; // Set pointer to start of the buffer

    // Now copy everything onto the buffer
    // ASSUME MEMCPY WORKS 

    // Copy the status line
    memcpy(
        cur, 
        response->status_line.http_version, 
        strlen(response->status_line.http_version)
    ); // http version
    cur += strlen(response->status_line.http_version);

    memcpy(cur, " ", 1); // white space
    cur += 1;

    char code_buf[4];
    snprintf(
        code_buf, 
        sizeof(code_buf), 
        "%03u", 
        response->status_line.status_code
    ); // Status code. honestly pretty awful
    memcpy(cur, code_buf, 3);
    cur += 3;

    memcpy(cur, " ", 1); // white space
    cur += 1;

    memcpy(
        cur, 
        response->status_line.reason_phrase,
        strlen(response->status_line.reason_phrase)
    ); // Status line
    cur += strlen(response->status_line.reason_phrase);

    memcpy(cur, "\r\n", 2); // Line ending \r\n
    cur += 2;

    // Headers
    if (response->headers != NULL) {
        for (uint32_t i = 0; i < response->headers->size; i++) {
            cur_header = ArrayList_get(response->headers, i);
            // Copy header contents
            memcpy(cur, cur_header->key, strlen(cur_header->key)); // key
            cur += strlen(cur_header->key);
            memcpy(cur, ": ", 2); // ": "
            cur += 2;
            memcpy(cur, cur_header->value, strlen(cur_header->value)); // value
            cur += strlen(cur_header->value);
            memcpy(cur, "\r\n", 2); // \r\n new line
            cur += 2;
        }
    }
    memcpy(cur, "\r\n", 2); // Final blank \r\n
    cur += 2;

    // Body
    if (response->body != NULL) {
        memcpy(cur, response->body, response->body_len);
        cur += response->body_len;
    } 

    // We are done
    *buf = ret_buf;
    *ret_len = len;

    return HTTP_UTIL_OK;
}

void print_status_line(Status_Line *status_line) {
    if (status_line == NULL) {
        fprintf(stderr, "print_status_line: status_line was NULL\n");
    } else {
        char *ver = status_line->http_version;
        int code = status_line->status_code;
        char *reason = status_line->reason_phrase;
    
        printf("status line:\n\thttp version: \"%s\" | status code: \"%d\" | reason phrase: \"%s\"\n", 
                ver, code, reason);
    }    
}

void print_response(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "print_response: request was NULL\n");
    } else {
        print_status_line(&response->status_line);        
        print_headers(response->headers);
        print_body(response->body, response->body_len);
    }
}

// TODO: Reorder these

int method_not_allowed(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "method_not_allowed: response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    response->status_line.http_version = HTTP_VER;
    response->status_line.status_code = HTTP_METNOALLOWED_405; // 405
    response->status_line.reason_phrase = "Method not allowed. Only valid method is GET";

    char *method_not_allowed_body = strdup("<h1>405 Method Not Allowed</h1>");
    if (method_not_allowed_body == NULL) {
        fprintf(stderr, "method_not_allowed: strdup fail for method_not_allowed_body\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    response->body = method_not_allowed_body; // Page contents
    response->body_len = strlen(response->body);
    init_headers(&response->headers); // Initialize headers
    add_header_content_length(response, response->body_len); // Add content length header
    
    return HTTP_UTIL_OK;
}

int internal_server_error(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "internal_server_error: response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    response->status_line.http_version = HTTP_VER;
    response->status_line.status_code = HTTP_SERVERR_500;
    response->status_line.reason_phrase = "Internal Server Error";

    response->body = NULL;
    response->body_len = 0;
    init_headers(&response->headers); // Initialize headers
    add_header_content_length(response, response->body_len); // Add content length header

    return HTTP_UTIL_OK;
}

int forbidden_request(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "forbidden_request: response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    response->status_line.http_version = HTTP_VER;
    response->status_line.status_code = 403;
    response->status_line.reason_phrase = "Forbidden. Request not allowed";

    char *forbidden_body = strdup("<h1>403 Forbidden</h1>");
    if (forbidden_body == NULL) {
        fprintf(stderr, "forbidden_request: strdup fail for forbidden_body\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    response->body = forbidden_body;
    response->body_len = strlen(response->body);
    init_headers(&response->headers); // Initialize headers
    add_header_content_length(response, response->body_len); // Add content length header

    return HTTP_UTIL_OK;
}

int not_found(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "not_found: response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    response->status_line.http_version = HTTP_VER;
    response->status_line.status_code = HTTP_NOTFOUND_404;
    response->status_line.reason_phrase = "Not found";

    char *not_bound_body = strdup("<h1>404 Not found</h1>");
    if (not_bound_body == NULL) {
        fprintf(stderr, "not_found: strdup fail for not_bound_body\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    response->body = not_bound_body;
    response->body_len = strlen(response->body);
    init_headers(&response->headers); // Initialize headers
    add_header_content_length(response, response->body_len); // Add content length header

    return HTTP_UTIL_OK;
}

// Returns 0 if is not a file. Non-zero otherwise
int is_file(const char *path) {
    if (path == NULL) {
        fprintf(stderr, "is_file: path was NULL\n");
        return 0;
    }

    struct stat p_stat;

    if (stat(path, &p_stat) != 0) {
        return 0;
    }

    // Returns non-zero if the path is a regular file
    return S_ISREG(p_stat.st_mode);
}

// Appent a content length header. EXPECTS A NON-NULL RESPONSE
// AND INITIALIZED CONTENT LENGTH ARRAY LIST
int add_header_content_length(Http_Response *response, int len) {
    if (response == NULL) {
        fprintf(stderr, "add_header_content_length: response was NULL\n");
        return HTTP_UTIL_BAD_INPUT;
    }

    Header *content_length_header = malloc(sizeof(Header));
    char *len_buff = malloc(20);

    if (content_length_header == NULL || len_buff == NULL) {
        fprintf(stderr, "add_header_content_length: malloc fail\n");
        return HTTP_UTIL_MEMALLOC_FAIL;
    }

    char* content_len = strdup("Content-Length");
    if (content_len == NULL) {
        fprintf(stderr, "add_header_content_length: strdup fail for content_len\n");
        free(content_length_header);
        free(len_buff);
        return HTTP_UTIL_MEMALLOC_FAIL;
    }    
    
    content_length_header->key = content_len;
    sprintf(len_buff, "%d", len);
    content_length_header->value = len_buff;

    ArrayList_insert(response->headers, content_length_header);

    return HTTP_UTIL_OK;
}

// ---- destructors ----

void free_request_line(Request_Line *request_line) {
    if (request_line == NULL) {
        fprintf(stderr, "free_request_line: request_line was NULL\n");
        return;
    }
    free(request_line->method);
    free(request_line->request_target);
    free(request_line->http_version);
}

void free_header(void *header) {
    free(((Header *)header)->key);
    free(((Header *)header)->value);
    free(header); 
}

void free_headers(ArrayList *headers) {
    if (headers == NULL) {
        fprintf(stderr, "free_headers: headers was NULL\n");
        return;
    }
    ArrayList_free(headers);
}

void free_request(Http_Request *request) {
    if (request == NULL) {
        fprintf(stderr, "free_request: request was NULL\n");
        return;
    }
    free_request_line(&request->request_line);
    free_headers(request->headers);
    free(request->body);
}

// DO NOT TRY TO FREE STATUS LINES. THEY POINT TO STATIC STRINGS
// void free_status_line(Status_Line *status_line) {
//     if (status_line == NULL) {
//         fprintf(stderr, "free_status_line: status_line was NULL\n");
//         return;
//     }
//     free(status_line->http_version);
//     free(status_line->reason_phrase);
// }

void free_response(Http_Response *response) {
    if (response == NULL) {
        fprintf(stderr, "free_response: response was NULL\n");
        return;
    }
    free_headers(response->headers);
    free(response->body);
}
