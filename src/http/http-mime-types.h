/*
copyright: Boaz segev, 2015
license: MIT

Feel free to copy, use and enjoy according to the license provided.
*/
#ifndef HTTP_MIME_TYPES_H
#define HTTP_MIME_TYPES_H

/**
The MimeType object allows us to easily translate a file extensions to mime-type
strings. i.e.:

    MimeType.find("mov"); // "video/quicktime"
*/
extern struct MimeType_API____ { char* (*find)(char* ext); } MimeType;

#endif /* HTTP_MIME_TYPES_H */
