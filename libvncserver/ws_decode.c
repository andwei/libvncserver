/*
 * ws_decode.c - decoding of websocket frames (RFC6455) into a byte stream
 * of raw data.
 *
 * This code should be independent of any changes in the RFB protocol. It is
 * an additional handshake and framing of normal sockets:
 *   http://www.whatwg.org/specs/web-socket-protocol/
 *
 */

/*
 *  Copyright (C) 2010-2017 Joel Martin, Andreas Weigel
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

#include "websockets.h"

#include <string.h>
#include <errno.h>

#undef WS_DECODE_DEBUG
/* set to 1 to produce very fine debugging output */
#define WS_DECODE_DEBUG 0

#if WS_DECODE_DEBUG == 1
#define ws_dbg(fmt, ...) rfbLog((fmt), ##__VA_ARGS)
#else
#define ws_dbg(fmt, ...)
#endif

void wsHeaderCleanup(ws_header_data_t *header)
{
  header->opcode = WS_OPCODE_INVALID;
  header->payloadLen = 0;
  header->mask.u = 0;
  header->headerLen = 0;
  header->data = NULL;
  header->nDone= 0;
}
static inline int
isControlFrame(ws_header_data_t *head)
{
  return 0 != (head->opcode & 0x08);
}

static uint64_t
remaining(ws_decoding_ctx_t *wsctx)
{
  return wsctx->header.payloadLen - wsctx->nReadPayload;
}

static void
wsDecodeCleanupBasics(ws_decoding_ctx_t *wsctx)
{
  wsHeaderCleanup(&(wsctx->header));
  wsctx->nReadPayload = 0;
  wsctx->carrylen = 0;
  wsctx->readPos = (unsigned char *)wsctx->codeBufDecode;
  wsctx->readlen = 0;
  wsctx->state = WS_STATE_DECODING_HEADER_PENDING;
  wsctx->writePos = NULL;
}

static void
wsDecodeCleanupForContinuation(ws_decoding_ctx_t *wsctx)
{
  wsDecodeCleanupBasics(wsctx);
  ws_dbg("clean up frame, but expect continuation with opcode %d\n", wsctx->continuation_opcode);
}

void
wsDecodeCleanupComplete(ws_decoding_ctx_t *wsctx)
{
  wsDecodeCleanupBasics(wsctx);
  wsctx->continuation_opcode = WS_OPCODE_INVALID;
  ws_dbg("cleaned up wsctx completely\n");
}


/**
 * Return payload data that has been decoded/unmasked from
 * a websocket frame.
 *
 * @param[out]     dst destination buffer
 * @param[in]      len bytes to copy to destination buffer
 * @param[in,out]  wsctx internal state of decoding procedure
 * @param[out]     number of bytes actually written to dst buffer
 * @return next decoding state
 */
static int
returnData(char *dst, int len, ws_decoding_ctx_t *wsctx, int *nWritten)
{
  int nextState = WS_STATE_ERR;

  /* if we have something already decoded copy and return */
  if (wsctx->readlen > 0) {
    /* simply return what we have */
    if (wsctx->readlen > len) {
      ws_dbg("copy to %d bytes to dst buffer; readPos=%p, readLen=%d\n", len, wsctx->readPos, wsctx->readlen);
      memcpy(dst, wsctx->readPos, len);
      *nWritten = len;
      wsctx->readlen -= len;
      wsctx->readPos += len;
      nextState = WS_STATE_DECODING_DATA_AVAILABLE;
    } else {
      ws_dbg("copy to %d bytes to dst buffer; readPos=%p, readLen=%d\n", wsctx->readlen, wsctx->readPos, wsctx->readlen);
      memcpy(dst, wsctx->readPos, wsctx->readlen);
      *nWritten = wsctx->readlen;
      wsctx->readlen = 0;
      wsctx->readPos = NULL;
      if (remaining(wsctx) == 0) {
        nextState = WS_STATE_DECODING_FRAME_COMPLETE;
      } else {
        nextState = WS_STATE_DECODING_DATA_NEEDED;
      }
    }
    ws_dbg("after copy: readPos=%p, readLen=%d\n", wsctx->readPos, wsctx->readlen);
  } else {
    /* it may happen that we read some bytes but could not decode them,
     * in that case, set errno to EAGAIN and return -1 */
    nextState = wsctx->state;
    errno = EAGAIN;
    *nWritten = -1;
  }
  return nextState;
}

/**
 * Read an RFC 6455 websocket frame (IETF hybi working group).
 *
 * Internal state is updated according to bytes received and the
 * decoding of header information.
 *
 * @param[in]   cl client ptr with ptr to raw socket and ws_decoding_ctx_t ptr
 * @param[out]  sockRet emulated recv return value
 * @param[out]  nPayload number of payload bytes already read
 * @return next decoding state; WS_STATE_DECODING_HEADER_PENDING indicates
 *         that the header was not received completely.
 */
static int
readHeader(ws_ctx_t *ctx, int *sockRet, int *nPayload)
{
  int ret;
  ws_decoding_ctx_t *wsctx = &ctx->dec;
  char *headerDst = wsctx->codeBufDecode + wsctx->header.nDone;
  int n = ((uint64_t)WSHLENMAX) - wsctx->header.nDone;

  ws_dbg("header_read to %p with len=%d\n", headerDst, n);
  ret = ctx->ctxInfo.readFunc(ctx->ctxInfo.ctxPtr, headerDst, n);
  ws_dbg("read %d bytes from socket\n", ret);
  if (ret <= 0) {
    if (-1 == ret) {
      /* save errno because rfbErr() will tamper it */
      int olderrno = errno;
      rfbErr("%s: read; %s\n", __func__, strerror(errno));
      errno = olderrno;
      goto err_cleanup_state;
    } else {
      *sockRet = 0;
      goto err_cleanup_state_sock_closed;
    }
  }

  wsctx->header.nDone += ret;
  if (wsctx->header.nDone < 2) {
    /* cannot decode header with less than two bytes */
    goto ret_header_pending;
  }

  /* first two header bytes received; interpret header data and get rest */
  wsctx->header.data = (ws_header_t *)wsctx->codeBufDecode;

  wsctx->header.opcode = wsctx->header.data->b0 & 0x0f;
  wsctx->header.fin = (wsctx->header.data->b0 & 0x80) >> 7;
  if (isControlFrame(&wsctx->header)) {
    ws_dbg("is control frame\n");
    /* is a control frame, leave remembered continuation opcode unchanged;
     * just check if there is a wrong fragmentation */
    if (wsctx->header.fin == 0) {

      /* we only accept text/binary continuation frames; RFC6455:
       * Control frames (see Section 5.5) MAY be injected in the middle of
       * a fragmented message.  Control frames themselves MUST NOT be
       * fragmented. */
      rfbErr("control frame with FIN bit cleared received, aborting\n");
      errno = EPROTO;
      goto err_cleanup_state;
    }
  } else {
    ws_dbg("not a control frame\n");
    /* not a control frame, check for continuation opcode */
    if (wsctx->header.opcode == WS_OPCODE_CONTINUATION) {
      ws_dbg("cont_frame\n");
      /* do we have state (i.e., opcode) for continuation frame? */
      if (wsctx->continuation_opcode == WS_OPCODE_INVALID) {
        rfbErr("no continuation state\n");
        errno = EPROTO;
        goto err_cleanup_state;
      }

      /* otherwise, set opcode = continuation_opcode */
      wsctx->header.opcode = wsctx->continuation_opcode;
      ws_dbg("set opcode to continuation_opcode: %d\n", wsctx->header.opcode);
    } else {
      if (wsctx->header.fin == 0) {
        wsctx->continuation_opcode = wsctx->header.opcode;
      } else {
        wsctx->continuation_opcode = WS_OPCODE_INVALID;
      }
      ws_dbg("set continuation_opcode to %d\n", wsctx->continuation_opcode);
    }
  }

  wsctx->header.payloadLen = (uint64_t)(wsctx->header.data->b1 & 0x7f);
  ws_dbg("first header bytes received; opcode=%d lenbyte=%d fin=%d\n", wsctx->header.opcode, wsctx->header.payloadLen, wsctx->header.fin);

  /*
   * 4.3. Client-to-Server Masking
   *
   * The client MUST mask all frames sent to the server.  A server MUST
   * close the connection upon receiving a frame with the MASK bit set to 0.
  **/
  if (!(wsctx->header.data->b1 & 0x80)) {
    rfbErr("%s: got frame without mask; ret=%d\n", __func__, ret);
    errno = EPROTO;
    goto err_cleanup_state;
  }


  if (wsctx->header.payloadLen < 126 && wsctx->header.nDone >= 6) {
    wsctx->header.headerLen = WS_HYBI_HEADER_LEN_SHORT_MASKED;
    wsctx->header.mask = wsctx->header.data->u.m;
  } else if (wsctx->header.payloadLen == 126 && 8 <= wsctx->header.nDone) {
    wsctx->header.headerLen = WS_HYBI_HEADER_LEN_EXTENDED_MASKED;
    wsctx->header.payloadLen = WS_NTOH16(wsctx->header.data->u.s16.l16);
    wsctx->header.mask = wsctx->header.data->u.s16.m16;
  } else if (wsctx->header.payloadLen == 127 && 14 <= wsctx->header.nDone) {
    wsctx->header.headerLen = WS_HYBI_HEADER_LEN_LONG_MASKED;
    wsctx->header.payloadLen = WS_NTOH64(wsctx->header.data->u.s64.l64);
    wsctx->header.mask = wsctx->header.data->u.s64.m64;
  } else {
    /* Incomplete frame header, try again */
    rfbErr("%s: incomplete frame header; ret=%d\n", __func__, ret);
    goto ret_header_pending;
  }

  char *h = wsctx->codeBufDecode;
  int i;
  ws_dbg("Header:\n");
  for (i=0; i <10; i++) {
    ws_dbg("0x%02X\n", (unsigned char)h[i]);
  }
  ws_dbg("\n");

  /* while RFC 6455 mandates that lengths MUST be encoded with the minimum
   * number of bytes, it does not specify for the server how to react on
   * 'wrongly' encoded frames --- this implementation rejects them*/
  if ((wsctx->header.headerLen > WS_HYBI_HEADER_LEN_SHORT_MASKED
      && wsctx->header.payloadLen < (uint64_t)126)
      || (wsctx->header.headerLen > WS_HYBI_HEADER_LEN_EXTENDED_MASKED
        && wsctx->header.payloadLen < (uint64_t)65536)) {
    rfbErr("%s: invalid length field; headerLen=%d payloadLen=%llu\n", __func__, wsctx->header.headerLen, wsctx->header.payloadLen);
    errno = EPROTO;
    goto err_cleanup_state;
  }

  /* update write position for next bytes */
  wsctx->writePos = wsctx->codeBufDecode + wsctx->header.nDone;

  /* set payload pointer just after header */
  wsctx->readPos = (unsigned char *)(wsctx->codeBufDecode + wsctx->header.headerLen);

  *nPayload = wsctx->header.nDone - wsctx->header.headerLen;
  wsctx->nReadPayload = *nPayload;

  ws_dbg("header complete: state=%d headerlen=%d payloadlen=%llu writeTo=%p nPayload=%d\n", wsctx->state, wsctx->header.headerLen, wsctx->header.payloadLen, wsctx->writePos, *nPayload);

  return WS_STATE_DECODING_DATA_NEEDED;

ret_header_pending:
  errno = EAGAIN;
  *sockRet = -1;
  return WS_STATE_DECODING_HEADER_PENDING;

err_cleanup_state:
  *sockRet = -1;
err_cleanup_state_sock_closed:
  wsDecodeCleanupComplete(wsctx);
  return WS_STATE_ERR;
}

static int
wsFrameComplete(ws_decoding_ctx_t *wsctx)
{
  return wsctx != NULL && remaining(wsctx) == 0;
}

static char *
payloadStart(ws_decoding_ctx_t *wsctx)
{
  return wsctx->codeBufDecode + wsctx->header.headerLen;
}


/**
 * Read the remaining payload bytes from associated raw socket.
 *
 *  - try to read remaining bytes from socket
 *  - unmask all multiples of 4
 *  - if frame incomplete but some bytes are left, these are copied to
 *      the carry buffer
 *  - if opcode is TEXT: Base64-decode all unmasked received bytes
 *  - set state for reading decoded data
 *  - reset write position to begin of buffer (+ header)
 *      --> before we retrieve more data we let the caller clear all bytes
 *          from the reception buffer
 *  - execute return data routine
 *
 *  Sets errno corresponding to what it gets from the underlying
 *  socket or EPROTO if some invalid data is in the received frame
 *  or ECONNRESET if a close reason + message is received. EIO is used if
 *  an internal sanity check fails.
 *
 *  @param[in]  cl client ptr with raw socket reference
 *  @param[out] dst  destination buffer
 *  @param[in]  len  size of destination buffer
 *  @param[out] sockRet emulated recv return value
 *  @param[in]  nInBuf number of undecoded bytes before writePos from header read
 *  @return next decode state
 */
static int
readAndDecode(ws_ctx_t *wsctx, char *dst, int len, int *sockRet, int nInBuf)
{
  int n;
  int i;
  int toReturn; /* number of data bytes to return */
  int toDecode; /* number of bytes to decode starting at dec_ctx->writePos */
  int bufsize;
  int nextRead;
  unsigned char *data;
  uint32_t *data32;
  ws_decoding_ctx_t *dec_ctx = &(wsctx->dec);

  /* if data was carried over, copy to start of buffer */
  memcpy(dec_ctx->writePos, dec_ctx->carryBuf, dec_ctx->carrylen);
  dec_ctx->writePos += dec_ctx->carrylen;

  /* -1 accounts for potential '\0' terminator for base64 decoding */
  bufsize = dec_ctx->codeBufDecode + ARRAYSIZE(dec_ctx->codeBufDecode) - dec_ctx->writePos - 1;
  ws_dbg("bufsize=%d\n", bufsize);
  if (remaining(dec_ctx) > bufsize) {
    nextRead = bufsize;
  } else {
    nextRead = remaining(dec_ctx);
  }

  ws_dbg("calling read with buf=%p and len=%d (decodebuf=%p headerLen=%d)\n", dec_ctx->writePos, nextRead, dec_ctx->codeBufDecode, dec_ctx->header.headerLen);

  if (nextRead > 0) {
    /* decode more data */
    if (-1 == (n = wsctx->ctxInfo.readFunc(wsctx->ctxInfo.ctxPtr, dec_ctx->writePos, nextRead))) {
      int olderrno = errno;
      rfbErr("%s: read; %s", __func__, strerror(errno));
      errno = olderrno;
      *sockRet = -1;
      return WS_STATE_ERR;
    } else if (n == 0) {
      *sockRet = 0;
      return WS_STATE_ERR;
    } else {
      ws_dbg("read %d bytes from socket; nRead=%d\n", n, dec_ctx->nReadPayload);
    }
  } else {
    n = 0;
  }

  dec_ctx->nReadPayload += n;
  dec_ctx->writePos += n;

  if (remaining(dec_ctx) == 0) {
    dec_ctx->state = WS_STATE_DECODING_FRAME_COMPLETE;
  }

  /* number of not yet unmasked payload bytes: what we read here + what was
   * carried over + what was read with the header */
  toDecode = n + dec_ctx->carrylen + nInBuf;
  ws_dbg("toDecode=%d from n=%d carrylen=%d headerLen=%d\n", toDecode, n, dec_ctx->carrylen, dec_ctx->header.headerLen);
  if (toDecode < 0) {
    rfbErr("%s: internal error; negative number of bytes to decode: %d", __func__, toDecode);
    errno=EIO;
    *sockRet = -1;
    return WS_STATE_ERR;
  }

  /* for a possible base64 decoding, we decode multiples of 4 bytes until
   * the whole frame is received and carry over any remaining bytes in the carry buf*/
  data = (unsigned char *)(dec_ctx->writePos - toDecode);
  data32= (uint32_t *)data;

  for (i = 0; i < (toDecode >> 2); i++) {
    data32[i] ^= dec_ctx->header.mask.u;
  }
  ws_dbg("mask decoding; i=%d toDecode=%d\n", i, toDecode);

  if (dec_ctx->state == WS_STATE_DECODING_FRAME_COMPLETE) {
    /* process the remaining bytes (if any) */
    for (i*=4; i < toDecode; i++) {
      data[i] ^= dec_ctx->header.mask.c[i % 4];
    }

    /* all data is here, no carrying */
    dec_ctx->carrylen = 0;
  } else {
    /* carry over remaining, non-multiple-of-four bytes */
    dec_ctx->carrylen = toDecode - (i * 4);
    if (dec_ctx->carrylen < 0 || dec_ctx->carrylen > ARRAYSIZE(dec_ctx->carryBuf)) {
      rfbErr("%s: internal error, invalid carry over size: carrylen=%d, toDecode=%d, i=%d", __func__, dec_ctx->carrylen, toDecode, i);
      *sockRet = -1;
      errno = EIO;
      return WS_STATE_ERR;
    }
    ws_dbg("carrying over %d bytes from %p to %p\n", dec_ctx->carrylen, dec_ctx->writePos + (i * 4), dec_ctx->carryBuf);
    memcpy(dec_ctx->carryBuf, data + (i * 4), dec_ctx->carrylen);
    dec_ctx->writePos -= dec_ctx->carrylen;
  }

  toReturn = toDecode - dec_ctx->carrylen;

  switch (dec_ctx->header.opcode) {
    case WS_OPCODE_CLOSE:
      /* this data is not returned as payload data */
      if (wsFrameComplete(dec_ctx)) {
        *(dec_ctx->writePos) = '\0';
        ws_dbg("got close cmd %d, reason %d: %s\n", (int)(dec_ctx->writePos - payloadStart(dec_ctx)), WS_NTOH16(((uint16_t *)payloadStart(dec_ctx))[0]), &payloadStart(dec_ctx)[2]);
        errno = ECONNRESET;
        *sockRet = -1;
        return WS_STATE_DECODING_FRAME_COMPLETE;
      } else {
        ws_dbg("got close cmd; waiting for %d more bytes to arrive\n", remaining(dec_ctx));
        *sockRet = -1;
        errno = EAGAIN;
        return WS_STATE_DECODING_CLOSE_REASON_PENDING;
      }
      break;
    case WS_OPCODE_TEXT_FRAME:
      data[toReturn] = '\0';
      ws_dbg("Initiate Base64 decoding in %p with max size %d and '\\0' at %p\n", data, bufsize, data + toReturn);
      if (-1 == (dec_ctx->readlen = b64_pton((char *)data, data, bufsize))) {
        rfbErr("%s: Base64 decode error; %s\n", __func__, strerror(errno));
      }
      dec_ctx->writePos = payloadStart(dec_ctx);
      break;
    case WS_OPCODE_BINARY_FRAME:
      dec_ctx->readlen = toReturn;
      dec_ctx->writePos = payloadStart(dec_ctx);
      ws_dbg("set readlen=%d writePos=%p\n", dec_ctx->readlen, dec_ctx->writePos);
      break;
    default:
      rfbErr("%s: unhandled opcode %d, b0: %02x, b1: %02x\n", __func__, (int)dec_ctx->header.opcode, dec_ctx->header.data->b0, dec_ctx->header.data->b1);
  }
  dec_ctx->readPos = data;

  return returnData(dst, len, dec_ctx, sockRet);
}

/**
 * Read function for websocket-socket emulation.
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-------+-+-------------+-------------------------------+
 *   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 *   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 *   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 *   | |1|2|3|       |K|             |                               |
 *   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 *   |     Extended payload length continued, if payload len == 127  |
 *   + - - - - - - - - - - - - - - - +-------------------------------+
 *   |                               |Masking-key, if MASK set to 1  |
 *   +-------------------------------+-------------------------------+
 *   | Masking-key (continued)       |          Payload Data         |
 *   +-------------------------------- - - - - - - - - - - - - - - - +
 *   :                     Payload Data continued ...                :
 *   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 *   |                     Payload Data continued ...                |
 *   +---------------------------------------------------------------+
 *
 * Using the decode buffer, this function:
 *  - reads the complete header from the underlying socket
 *  - reads any remaining data bytes
 *  - unmasks the payload data using the provided mask
 *  - decodes Base64 encoded text data
 *  - copies len bytes of decoded payload data into dst
 *
 * Emulates a read call on a socket.
 */
int
_webSocketsDecode(ws_ctx_t *wsctx, char *dst, int len)
{
    int result = -1;
    ws_decoding_ctx_t *dec_ctx = &(wsctx->dec);
    ws_dbg("%s_enter: len=%d; "
                      "CTX: readlen=%d readPos=%p "
                      "writeTo=%p "
                      "state=%d payloadtoRead=%d payloadRemaining=%llu "
                      " nReadPayload=%d carrylen=%d carryBuf=%p\n",
                      __func__, len,
                      dec_ctx->readlen, dec_ctx->readPos,
                      dec_ctx->writePos,
                      dec_ctx->state, dec_ctx->header.payloadLen, remaining(dec_ctx),
                      dec_ctx->nReadPayload, dec_ctx->carrylen, dec_ctx->carryBuf);

    switch (dec_ctx->state){
      int nInBuf;
      case WS_STATE_DECODING_HEADER_PENDING:
        dec_ctx->state = readHeader(wsctx, &result, &nInBuf);
        if (dec_ctx->state == WS_STATE_ERR) {
          goto spor;
        }
        if (dec_ctx->state != WS_STATE_DECODING_HEADER_PENDING) {

          /* when header is complete, try to read some more data */
          dec_ctx->state = readAndDecode(wsctx, dst, len, &result, nInBuf);
        }
        break;
      case WS_STATE_DECODING_DATA_AVAILABLE:
        dec_ctx->state = returnData(dst, len, dec_ctx, &result);
        break;
      case WS_STATE_DECODING_DATA_NEEDED:
        dec_ctx->state = readAndDecode(wsctx, dst, len, &result, 0);
        break;
      case WS_STATE_DECODING_CLOSE_REASON_PENDING:
        dec_ctx->state = readAndDecode(wsctx, dst, len, &result, 0);
        break;
      default:
        /* invalid state */
        rfbErr("%s: called with invalid state %d\n", dec_ctx->state);
        result = -1;
        errno = EIO;
        dec_ctx->state = WS_STATE_ERR;
    }

    /* single point of return, if someone has questions :-) */
spor:
    if (dec_ctx->state == WS_STATE_DECODING_FRAME_COMPLETE) {
      ws_dbg("frame received successfully, cleaning up: read=%d hlen=%d plen=%d\n", dec_ctx->nReadPayload, dec_ctx->header.headerLen, dec_ctx->header.payloadLen);
      if (dec_ctx->header.fin && !isControlFrame(&dec_ctx->header)) {
        /* frame finished, cleanup state */
        wsDecodeCleanupComplete(dec_ctx);
      } else {
        /* always retain continuation opcode for unfinished data frames
         * or control frames, which may interleave with data frames */
        wsDecodeCleanupForContinuation(dec_ctx);
      }
    } else if (dec_ctx->state == WS_STATE_ERR) {
      wsDecodeCleanupComplete(dec_ctx);
    }

    ws_dbg("%s_exit: len=%d; "
                      "CTX: readlen=%d readPos=%p "
                      "writePos=%p "
                      "state=%d payloadtoRead=%d payloadRemaining=%d "
                      "nRead=%d carrylen=%d carryBuf=%p "
                      "result=%d "
                      "errno=%d\n",
                      __func__, len,
                      dec_ctx->readlen, dec_ctx->readPos,
                      dec_ctx->writePos,
                      dec_ctx->state, dec_ctx->header.payloadLen, remaining(dec_ctx),
                      dec_ctx->nReadPayload, dec_ctx->carrylen, dec_ctx->carryBuf,
                      result,
                      errno);
    return result;
}
