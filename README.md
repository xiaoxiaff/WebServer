# WebServer

This is a web server. First we need to implement the basic response message. Then we can start to serve different types of request.

## Response 

Based on RFC1945, there are two types of Response: Simple-Response and Full-Response.

Simple-Response = [ Entity-Body ]

Full-Response = Status-Line             : Section 6.1
                *( General-Header       ; Section 4.3
                 | Response-Header      ; Section 6.2
                 | Entity-Header )      ; Section 7.1
                CRLF
                [ Entity-Body ]         ; Section 7.2

For different header, we have:

General-Header = Date                     ; Section 10.6
               | Pragma                   ; Section 10.12

Response-Header = Location                ; Section 10.11
                | Server                  ; Section 10.14
                | WWW-Authenticate        ; Section 10.16

Entity-Header  = Allow                    ; Section 10.1
               | Content-Encoding         ; Section 10.3
               | Content-Length           ; Section 10.4
               | Content-Type             ; Section 10.5
               | Expires                  ; Section 10.7
               | Last-Modified            ; Section 10.10
               | extension-header

extension-header = HTTP-header
