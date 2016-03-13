/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_STATUS_H
#define HTTP_STATUS_H
/**
The HttpStatus object allows us to easily translate a numerical HTTP status code
to a String. i.e.:

    HttpStatus.to_s(200); // "OK"
*/
extern struct HttpStatus_API____ { char* (*to_s)(int status); } HttpStatus;

#endif /* HTTP_PROTOCOL_H */
