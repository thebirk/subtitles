#include "json.h"

#define UNICODE
#include <winsock2.h>
#include <Windows.h>
#include <http.h>
#include <dwmapi.h>
#include <CommCtrl.h>
#pragma comment(lib, "Httpapi.lib")
#pragma comment(lib, "dwmapi.lib")

#include <stdio.h>

DWORD
SendHttpPostResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
);

DWORD
SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntity
);

#define WM_SUBTITLE_UPDATED (WM_USER+1)

LPWSTR str = L"";
int strLen = 0;

void Draw(HDC dc)
{
    SelectObject(dc, CreateFont(48, 0, 0, 0, 0, FALSE, FALSE, FALSE, 0, 0, 0, 0, 0, L"Consolas"));
    TextOut(dc, 400, 400, str, strLen);
}

DWORD RenderThread(LPVOID param)
{
    for (;;) {
        MSG msg;
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&msg);

            switch (msg.message) {
            case WM_SUBTITLE_UPDATED: {
                printf("WM_SUBTITLE_UPDATED\n");

                int wideLen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)msg.wParam, msg.lParam, 0, 0);
                LPWSTR wideBuffer = (LPWCH)HeapAlloc(GetProcessHeap(), 0, (sizeof(WCHAR) * wideLen));
                if (!wideBuffer) continue;
                MultiByteToWideChar(CP_UTF8, 0, (LPCCH)msg.wParam, msg.lParam, wideBuffer, wideLen);

                str = wideBuffer;
                strLen = wideLen;
                HDC dc = GetDC(0);
                Draw(dc);
            } break;
            }

            DispatchMessage(&msg);
        }

        DwmFlush();
        HDC dc = GetDC(0);
        Draw(dc);
    }

    return 0;
}

HFONT font = CreateFont(48, 0, 0, 0, 0, FALSE, FALSE, FALSE, 0, 0, 0, 0, 0, L"Consolas");
HWND wnd;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_PAINT: {
        HDC dc = GetDC(hwnd);
        SelectObject(dc, font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        //TextOut(dc, 2560/2, 1440-200, L"Hello", 5);

        RECT rect = {};
        GetClientRect(hwnd, &rect);
        rect.left = 200;
        rect.top = 1440 - 800;
        DrawText(dc, str, strLen, &rect, DT_CENTER | DT_NOCLIP);

        ReleaseDC(hwnd, dc);
    } break;
    
    case WM_SUBTITLE_UPDATED: {
        int wideLen = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)wparam, lparam, 0, 0);
        LPWSTR wideBuffer = (LPWCH)HeapAlloc(GetProcessHeap(), 0, (sizeof(WCHAR) * wideLen));
        if (!wideBuffer) break;
        MultiByteToWideChar(CP_UTF8, 0, (LPCCH)wparam, lparam, wideBuffer, wideLen);

        str = wideBuffer;
        strLen = wideLen;

        InvalidateRect(hwnd, 0, TRUE);
    } break;

    case WM_NCACTIVATE: {
        result = 0;
        break;
    }

    default:
        result = DefWindowProc(hwnd, msg, wparam, lparam);
        break;
    }

    return result;
}

DWORD WindowThread(LPVOID param)
{
    WNDCLASS dummyParentClass = {};
    dummyParentClass.lpfnWndProc = DefWindowProc;
    dummyParentClass.lpszClassName = L"dummyParent";
    RegisterClass(&dummyParentClass);

    HWND dummyParent = CreateWindowW(L"dummyParent", L"", 0, 0, 0, 0, 0, 0, 0, 0, 0);

    WNDCLASSW wndClass = {};
    wndClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    wndClass.lpfnWndProc = wndProc;
    wndClass.lpszClassName = L"mywindow";
    wndClass.hbrBackground = CreateSolidBrush(RGB(255, 0, 255));
    RegisterClass(&wndClass);

    wnd = CreateWindowW(L"mywindow", L"nope", WS_POPUP|WS_VISIBLE, 200, 200, 500, 500, dummyParent, 0, 0, 0);
    SetWindowLongW(wnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
    SetLayeredWindowAttributes(wnd, RGB(255, 0, 255), 0, LWA_COLORKEY);

    MONITORINFO monitor = {};
    GetMonitorInfo(MonitorFromWindow(wnd, MONITOR_DEFAULTTOPRIMARY), &monitor);
    SetWindowPos(wnd, HWND_TOPMOST, 0, 0, 2560, 1440, 0);
    

    MSG msg;
    while (GetMessage(&msg, wnd, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

int main()
{
    DWORD renderThreadId = 0;
    //CreateThread(0, 0, RenderThread, 0, 0, &renderThreadId);
    CreateThread(0, 0, WindowThread, 0, 0, 0);

    HttpInitialize(HTTPAPI_VERSION_1, HTTP_INITIALIZE_SERVER, 0);

    HANDLE requestQueue;
    HttpCreateHttpHandle(&requestQueue, 0);
    HttpAddUrl(requestQueue, L"http://localhost:4007/subtitles", 0);

    HTTP_REQUEST_ID reqId;
    HTTP_SET_NULL_ID(&reqId);
    int requestBufferLength = sizeof(HTTP_REQUEST) + 2048;
    PHTTP_REQUEST request = (PHTTP_REQUEST)HeapAlloc(GetProcessHeap(), 0, requestBufferLength);
    if (!request) return 123;
    DWORD bytesRead;

    loop_start:;
    for (;;) {
        RtlZeroMemory(request, requestBufferLength);

        OutputDebugStringA("Waiting for request..\n");
        HttpReceiveHttpRequest(requestQueue, reqId, HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY, request, requestBufferLength, &bytesRead, NULL);
        switch (request->Verb) {
        case HttpVerbPOST: {
            if (wcscmp(L"/subtitles", request->CookedUrl.pAbsPath) == 0) {
                if (!request->pEntityChunks || request->pEntityChunks->DataChunkType != HttpDataChunkFromMemory) goto error;

                auto value = json_parse(request->pEntityChunks->FromMemory.pBuffer, request->pEntityChunks->FromMemory.BufferLength);

                /*
                if (value->type != json_type_array) goto error;

                json_array_s* array = (json_array_s*)value->payload;
                if (!array) goto error;
                for (json_array_element_s* it = array->start; it != nullptr; it = it->next) {
                    if (!it->value) goto error;
                    if (it->value->type != json_type_string) goto error;

                    auto string = (json_string_s*)it->value->payload;
                    printf("%.*s", (unsigned int)string->string_size, string->string);
                    if (!it->next) printf("\n");
                    PostMessage(wnd, WM_SUBTITLE_UPDATED, (WPARAM)string->string, string->string_size);
                }
                */

                if (value->type != json_type_string) goto error;

                auto string = (json_string_s*)value->payload;
                printf("%.*s", (unsigned int)string->string_size, string->string);
                PostMessage(wnd, WM_SUBTITLE_UPDATED, (WPARAM)string->string, string->string_size);
            }

            SendHttpPostResponse(requestQueue, request);
        } break;
        case HttpVerbOPTIONS: {
            SendHttpResponse(requestQueue, request, 204, "No Content", NULL);
        } break;
        default: {
        error:;
            SendHttpResponse(requestQueue, request, 503, "Not Implemented", NULL);
        } break;
        }

        HTTP_SET_NULL_ID(&reqId);
    }
    return 0;
}










////////////////
#define MAX_ULONG_STR ((ULONG) sizeof("4294967295"))

//
// Macros.
//
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)               \
    do                                                               \
    {                                                                \
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue =      \
                                                          (RawValue);\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = \
            (USHORT) strlen(RawValue);                               \
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))


#define RESERVE_UNKNOWN_HEADERS(response, count) \
    response.Headers.UnknownHeaderCount = count; \
    response.Headers.pUnknownHeaders = (PHTTP_UNKNOWN_HEADER)ALLOC_MEM(sizeof(HTTP_UNKNOWN_HEADER) * count);

#define SET_UNKNOWN_HEADER(response, index, name, value) \
    response.Headers.pUnknownHeaders[index].NameLength = sizeof(name) - 1; \
    response.Headers.pUnknownHeaders[index].pName = (char*)ALLOC_MEM(response.Headers.pUnknownHeaders[index].NameLength); \
    memcpy_s((void*)response.Headers.pUnknownHeaders[index].pName, sizeof(name)-1, name, sizeof(name)-1); \
 \
    response.Headers.pUnknownHeaders[index].RawValueLength = sizeof(value) - 1; \
    response.Headers.pUnknownHeaders[index].pRawValue = (char*)ALLOC_MEM(response.Headers.pUnknownHeaders[index].RawValueLength); \
    memcpy_s((void*)response.Headers.pUnknownHeaders[index].pRawValue, sizeof(value)-1, value, sizeof(value)-1);


/*******************************************************************++

Routine Description:
    The routine sends a HTTP response

Arguments:
    hReqQueue     - Handle to the request queue
    pRequest      - The parsed HTTP request
    StatusCode    - Response Status Code
    pReason       - Response reason phrase
    pEntityString - Response entity body

Return Value:
    Success/Failure.
--*******************************************************************/

DWORD SendHttpResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest,
    IN USHORT        StatusCode,
    IN PSTR          pReason,
    IN PSTR          pEntityString
)
{
    HTTP_RESPONSE   response;
    HTTP_DATA_CHUNK dataChunk;
    DWORD           result;
    DWORD           bytesSent;

    //
    // Initialize the HTTP response structure.
    //
    INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

    //
    // Add a known header.
    //
    ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");

    RESERVE_UNKNOWN_HEADERS(response, 3);
    SET_UNKNOWN_HEADER(response, 0, "Access-Control-Allow-Origin", "*");
    SET_UNKNOWN_HEADER(response, 1, "Access-Control-Allow-Headers", "*");
    SET_UNKNOWN_HEADER(response, 2, "Access-Control-Allow-Methods", "*");


    if (pEntityString)
    {
        // 
        // Add an entity chunk.
        //
        dataChunk.DataChunkType = HttpDataChunkFromMemory;
        dataChunk.FromMemory.pBuffer = pEntityString;
        dataChunk.FromMemory.BufferLength =
            (ULONG)strlen(pEntityString);

        response.EntityChunkCount = 1;
        response.pEntityChunks = &dataChunk;
    }

    // 
    // Because the entity body is sent in one call, it is not
    // required to specify the Content-Length.
    //

    result = HttpSendHttpResponse(
        hReqQueue,           // ReqQueueHandle
        pRequest->RequestId, // Request ID
        0,                   // Flags
        &response,           // HTTP response
        NULL,                // pReserved1
        &bytesSent,          // bytes sent  (OPTIONAL)
        NULL,                // pReserved2  (must be NULL)
        0,                   // Reserved3   (must be 0)
        NULL,                // LPOVERLAPPED(OPTIONAL)
        NULL                 // pReserved4  (must be NULL)
    );

    if (result != NO_ERROR)
    {
        wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
    }

    return result;
}

/*******************************************************************++

Routine Description:
    The routine sends a HTTP response after reading the entity body.

Arguments:
    hReqQueue     - Handle to the request queue.
    pRequest      - The parsed HTTP request.

Return Value:
    Success/Failure.
--*******************************************************************/

DWORD SendHttpPostResponse(
    IN HANDLE        hReqQueue,
    IN PHTTP_REQUEST pRequest
)
{
    HTTP_RESPONSE   response;
    DWORD           result;
    DWORD           bytesSent;
    PUCHAR          pEntityBuffer;
    ULONG           EntityBufferLength;
    ULONG           BytesRead;
    ULONG           TempFileBytesWritten;
    HANDLE          hTempFile;
    TCHAR           szTempName[MAX_PATH + 1];
    CHAR            szContentLength[MAX_ULONG_STR];
    HTTP_DATA_CHUNK dataChunk;
    ULONG           TotalBytesRead = 0;

    BytesRead = 0;
    hTempFile = INVALID_HANDLE_VALUE;

    //
    // Allocate space for an entity buffer. Buffer can be increased 
    // on demand.
    //
    EntityBufferLength = 2048;
    pEntityBuffer = (PUCHAR)ALLOC_MEM(EntityBufferLength);

    if (pEntityBuffer == NULL)
    {
        result = ERROR_NOT_ENOUGH_MEMORY;
        wprintf(L"Insufficient resources \n");
        goto Done;
    }

    //
    // Initialize the HTTP response structure.
    //
    INITIALIZE_HTTP_RESPONSE(&response, 200, "OK");

    //
    // For POST, echo back the entity from the
    // client
    //
    // NOTE: If the HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY flag had been
    //       passed with HttpReceiveHttpRequest(), the entity would 
    //       have been a part of HTTP_REQUEST (using the pEntityChunks
    //       field). Because that flag was not passed, there are no
    //       o entity bodies in HTTP_REQUEST.
    //

    if (pRequest->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS)
    {
        // The entity body is sent over multiple calls. Collect 
        // these in a file and send back. Create a temporary 
        // file.
        //

        if (GetTempFileName(
            L".",
            L"New",
            0,
            szTempName
        ) == 0)
        {
            result = GetLastError();
            wprintf(L"GetTempFileName failed with %lu \n", result);
            goto Done;
        }

        hTempFile = CreateFile(
            szTempName,
            GENERIC_READ | GENERIC_WRITE,
            0,                  // Do not share.
            NULL,               // No security descriptor.
            CREATE_ALWAYS,      // Overrwrite existing.
            FILE_ATTRIBUTE_NORMAL,    // Normal file.
            NULL
        );

        if (hTempFile == INVALID_HANDLE_VALUE)
        {
            result = GetLastError();
            wprintf(L"Cannot create temporary file. Error %lu \n",
                result);
            goto Done;
        }

        do
        {
            //
            // Read the entity chunk from the request.
            //
            BytesRead = 0;
            result = HttpReceiveRequestEntityBody(
                hReqQueue,
                pRequest->RequestId,
                0,
                pEntityBuffer,
                EntityBufferLength,
                &BytesRead,
                NULL
            );

            switch (result)
            {
            case NO_ERROR:

                if (BytesRead != 0)
                {
                    TotalBytesRead += BytesRead;
                    WriteFile(
                        hTempFile,
                        pEntityBuffer,
                        BytesRead,
                        &TempFileBytesWritten,
                        NULL
                    );
                }
                break;

            case ERROR_HANDLE_EOF:

                //
                // The last request entity body has been read.
                // Send back a response. 
                //
                // To illustrate entity sends via 
                // HttpSendResponseEntityBody, the response will 
                // be sent over multiple calls. To do this,
                // pass the HTTP_SEND_RESPONSE_FLAG_MORE_DATA
                // flag.

                if (BytesRead != 0)
                {
                    TotalBytesRead += BytesRead;
                    WriteFile(
                        hTempFile,
                        pEntityBuffer,
                        BytesRead,
                        &TempFileBytesWritten,
                        NULL
                    );
                }

                //
                // Because the response is sent over multiple
                // API calls, add a content-length.
                //
                // Alternatively, the response could have been
                // sent using chunked transfer encoding, by  
                // passimg "Transfer-Encoding: Chunked".
                //

                // NOTE: Because the TotalBytesread in a ULONG
                //       are accumulated, this will not work
                //       for entity bodies larger than 4 GB. 
                //       For support of large entity bodies,
                //       use a ULONGLONG.
                // 


                sprintf_s(szContentLength, MAX_ULONG_STR, "%lu", TotalBytesRead);

                ADD_KNOWN_HEADER(
                    response,
                    HttpHeaderContentLength,
                    szContentLength
                );

                result =
                    HttpSendHttpResponse(
                        hReqQueue,           // ReqQueueHandle
                        pRequest->RequestId, // Request ID
                        HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
                        &response,       // HTTP response
                        NULL,            // pReserved1
                        &bytesSent,      // bytes sent-optional
                        NULL,            // pReserved2
                        0,               // Reserved3
                        NULL,            // LPOVERLAPPED
                        NULL             // pReserved4
                    );

                if (result != NO_ERROR)
                {
                    wprintf(
                        L"HttpSendHttpResponse failed with %lu \n",
                        result
                    );
                    goto Done;
                }

                //
                // Send entity body from a file handle.
                //
                dataChunk.DataChunkType =
                    HttpDataChunkFromFileHandle;

                dataChunk.FromFileHandle.
                    ByteRange.StartingOffset.QuadPart = 0;

                dataChunk.FromFileHandle.
                    ByteRange.Length.QuadPart =
                    HTTP_BYTE_RANGE_TO_EOF;

                dataChunk.FromFileHandle.FileHandle = hTempFile;

                result = HttpSendResponseEntityBody(
                    hReqQueue,
                    pRequest->RequestId,
                    0,           // This is the last send.
                    1,           // Entity Chunk Count.
                    &dataChunk,
                    NULL,
                    NULL,
                    0,
                    NULL,
                    NULL
                );

                if (result != NO_ERROR)
                {
                    wprintf(
                        L"HttpSendResponseEntityBody failed %lu\n",
                        result
                    );
                }

                goto Done;

                break;


            default:
                wprintf(
                    L"HttpReceiveRequestEntityBody failed with %lu \n",
                    result);
                goto Done;
            }

        } while (TRUE);
    }
    else
    {
        // This request does not have an entity body.
        //

        result = HttpSendHttpResponse(
            hReqQueue,           // ReqQueueHandle
            pRequest->RequestId, // Request ID
            0,
            &response,           // HTTP response
            NULL,                // pReserved1
            &bytesSent,          // bytes sent (optional)
            NULL,                // pReserved2
            0,                   // Reserved3
            NULL,                // LPOVERLAPPED
            NULL                 // pReserved4
        );
        if (result != NO_ERROR)
        {
            wprintf(L"HttpSendHttpResponse failed with %lu \n",
                result);
        }
    }

Done:

    if (pEntityBuffer)
    {
        FREE_MEM(pEntityBuffer);
    }

    if (INVALID_HANDLE_VALUE != hTempFile)
    {
        CloseHandle(hTempFile);
        DeleteFile(szTempName);
    }

    return result;
}