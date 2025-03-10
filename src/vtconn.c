/* Copyright (C) 1996 Office Products Technology, Inc.

This file is part of FreeVT3k.

FreeVT3k is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

FreeVT3k is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with FreeVT3k. If not, see <https://www.gnu.org/licenses/>.
*/

/************************************************************
 * vtconn.c -- Connection handling
 ************************************************************/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "dumpbuf.h"
#include "vt3kglue.h"

extern int
	debug;
int
	appl_req_count = 0;


static void DefaultDataOutProc(int32_t refCon, char * buffer, size_t bufferLength)
{ /*DefaultDataOutProc*/
    if (write(STDOUT_FILENO, buffer, bufferLength)) {}
} /*DefaultDataOutProc*/

static int SendToAM(tVTConnection * conn, 
                      tVTMHeader * theMessage, uint16_t messageLength)
{ /*SendToAM*/
    int   returnValue = kVTCNoError;
    ssize_t
	charsSent;

    theMessage->fMessageLength = htons(messageLength);
    if (debug > 0)
	DumpBuffer(&theMessage->fProtocolID,
		   messageLength-sizeof(theMessage->fMessageLength),
		   "to_host");
    charsSent = send(conn->fSocket, (void*)theMessage, messageLength, 0);
    if (charsSent == -1)
        {
        returnValue = kVTCSocketError;
        conn->fLastSocketError = errno;
        }
    else if (charsSent != messageLength)
        {
        returnValue = kVTCSendError;
        }
    return returnValue;
} /*SendToAM*/

static void FillStandardMessageHeader(tVTMHeader * theHeader, 
				       tVTMessageType messageType,
				       uint8_t primitive)	/* RM 960410 */
{ /*FillStandardMessageHeader*/
    theHeader->fProtocolID = kVTProtocolID;
    theHeader->fMessageType = messageType;
    theHeader->fPrimitive = primitive;
    theHeader->fUnused = 0;
}

static void SetUpForNewRecordReceive(tVTConnection * conn)
{ /*SetUpForNewRecordReceive*/
    conn->fLengthToReceive = 2;
    conn->fReceiveBufferOffset = -2;
} /*SetUpForNewRecordReceive*/

static int ProcessAMNegotiationRequest(tVTConnection * conn)
{ /*ProcessAMNegotiationRequest*/
    int returnValue = kVTCNoError;
    tVTMAMNegotiationRequest * amreq = (tVTMAMNegotiationRequest *)
						conn->fReceiveBuffer;
    tVTMAMNegotiationReply      amresp;
    tVTMTMNegotiationRequest    tmreq;
    char	hostName[128];
    char	domainName[128];
    uint16_t	nodeNameLength;
    tVTMAMBreakInfo		*breakInfo;
    char	pid_buf[sizeof(tmreq.fSessionID)+1];
    /* First, convert all multi-byte values to host order. */

#define TOHOST(f) amreq->f = ntohs(amreq->f)

    TOHOST(fRequestCount);
    TOHOST(fOperatingSystem);
    TOHOST(fBufferSize);
    TOHOST(fTypeAheadSize);
    TOHOST(fBreakOffset);
    TOHOST(fBreakIndexCount);
    TOHOST(fLogonIDOffset);
    TOHOST(fLogonIDLength);
    TOHOST(fDeviceIDOffset);
    TOHOST(fDeviceIDLength);
    TOHOST(fLineDeleteOffset);
    TOHOST(fLineDeleteLength);
    TOHOST(fAMMaxReceiveBurst);
    TOHOST(fAMMaxSendBurst);

    /* Next validate the information if possible and store the state */
    /* information requested by the AM.				     */

    conn->fEcho = amreq->fEcho;		/* What is this?	*/
    conn->fEchoControl = amreq->fEchoControl;
    conn->fCharDeleteChar = amreq->fCharacterDelete;
    conn->fCharDeleteEcho = amreq->fCharacterDeleteEcho;
    conn->fLineDeleteChar = amreq->fLineDeleteCharacter;
    if (amreq->fLineDeleteLength != 0 &&
        amreq->fLineDeleteOffset != 0)
        {
        if (amreq->fLineDeleteLength < kMaxLineDeleteEcho)
            conn->fLineDeleteEchoLength = amreq->fLineDeleteLength;
        else conn->fLineDeleteEchoLength = kMaxLineDeleteEcho;
        memcpy(conn->fLineDeleteEcho,
	       amreq->fLineDeleteOffset + ((char *) amreq), 
	       conn->fLineDeleteEchoLength);
        }
    
    conn->fNoBreakRead = amreq->fNoBreakRead; /* What is this? */

    /* Not sure if this is correct. Is the HP telling us what its buffer size
       is, or is it telling us what it wants our buffer size to be? */
    if ((int)amreq->fBufferSize < conn->fSendBufferSize)
	conn->fSendBufferSize = amreq->fBufferSize;

    if (amreq->fBreakOffset)
        {
	breakInfo = (tVTMAMBreakInfo*)((uint16_t*)amreq + amreq->fBreakOffset/sizeof(uint16_t));
        conn->fSysBreakEnabled = (breakInfo->fSysBreakEnabled) ? true : false;
        conn->fSubsysBreakEnabled = (breakInfo->fSubsysBreakEnabled) ? true : false;
        conn->fSysBreakChar = ntohs(breakInfo->fSysBreakChar);
        conn->fSubsysBreakChar = ntohs(breakInfo->fSubsysBreakChar);
        }
        
    /* Put together and send a reply packet. */

    memset((char *) &amresp, 0, sizeof(amresp));
    FillStandardMessageHeader((tVTMHeader *) &amresp, 
				kvmtEnvCntlResp, kvtpAMNegotiate);
    amresp.fRequestCount = htons(amreq->fRequestCount);
    amresp.fResponseCode = htons(kAMNRSuccessful);
    amresp.fCompletionMask = htons(kAMNRSuccessful);
    amresp.fBufferSize = htons(conn->fSendBufferSize);
    amresp.fVersionMask[0] = (unsigned char)0xd0;	/* Just copied */
    amresp.fVersionMask[3] = 100;       /* Official ID for freevt3k */
    amresp.fOperatingSystem = htons(kReturnOSType);
    amresp.fHardwareControlCompletionMask = 
		htons(kHWCTLParity);   
    amresp.fTMMaxReceiveBurst = htons(amreq->fAMMaxSendBurst);
    amresp.fTMMaxSendBurst = htons(amreq->fAMMaxReceiveBurst);
    if ((returnValue = SendToAM(conn, (tVTMHeader *) &amresp,
				sizeof(amresp)))) goto Last;

    /* if we are/were waiting for AM's initial negotiation request... */

    if (conn->fState == kvtsWaitingForAM) {

	/* put together and send an initial TM negotiation request */ 

	FillStandardMessageHeader((tVTMHeader *) &tmreq, 
				  kvmtEnvCntlReq, kvtpTMNegotiate);
	tmreq.fRequestCount = htons(++conn->fCurrentTMRequestCount);
	tmreq.fLinkType = kVTDefaultCommLinkType;
	tmreq.fTerminalClass = htons(kVTDefaultTerminalClass);

	sprintf(pid_buf, "%0*d", (int) sizeof(tmreq.fSessionID), getpid());
	memcpy(tmreq.fSessionID, pid_buf, sizeof(tmreq.fSessionID));

	gethostname(hostName, sizeof(hostName));
	if (getdomainname(domainName, sizeof(domainName)) != 0) {
        domainName[0] = 0;
    } else {
	    strcat(hostName, ".");
	    strcat(hostName, domainName);
    }

	nodeNameLength = strlen(hostName);
	if (nodeNameLength > sizeof(tmreq.fNodeName))
	    nodeNameLength = sizeof(tmreq.fNodeName);
	tmreq.fNodeLength = htons(nodeNameLength);
	memset((char *)tmreq.fNodeName, 0, sizeof(tmreq.fNodeName));
	memcpy(tmreq.fNodeName, hostName, nodeNameLength);
	if ((returnValue = SendToAM(conn, (tVTMHeader *) &tmreq, sizeof(tmreq))))
		goto Last;

	/* and now we are done waiting for AM negotiation and instead waiting for
	 * TM negotiation reply
	 */
	conn->fState = kvtsWaitingForTMReply;
    }
Last:
    return returnValue;
#undef TOHOST
} /*ProcessAMNegotiationRequest*/

static int ProcessTMNegotiationResponse(tVTConnection * conn)
{ /*ProcessTMNegotiationResponse*/
   tVTMTMNegotiationReply * tmResp = 
                  (tVTMTMNegotiationReply *) conn->fReceiveBuffer;
   int	returnValue = kVTCNoError;

   if (ntohs(tmResp->fResponseCode) != kTMNRSuccessful)
	returnValue = kVTCRejectedTMNegotiation;
   else {
	returnValue = kVTCVTOpen;
	conn->fState = kvtsOpen;
   }

   return returnValue;
} /*ProcessTMNegotiationResponse*/

static int ProcessTerminationResponse(tVTConnection * conn)
{ /*ProcessTerminationResponse*/
    int returnValue = kVTCNoError;

    /* What to do if the termination request is denied? */

    returnValue = kVTCStartShutdown;

    return returnValue;
} /*ProcessTerminationResponse*/

static void ProcessCCTL(tVTConnection * conn, unsigned char cctlChar)
{ /*ProcessCCTL*/
    char  lfBuffer[80];
    char  *ptr;
    long  len = 0;

#define CR	(0x0D)
#define LF	(0x0A)
#define FF	(0x0C)
/* NOCCTL - nothing else to do */
    if (cctlChar == 0320)
        return;
    if ((0200 <= cctlChar) && (cctlChar <= 0277))	/* Skip 'n' lines */
	len = cctlChar - 0200;
    else
	{
	switch (cctlChar)
            {
	case 053:	len = 0; break;	/* CR only */
	case 060:	len = 2; break;	/* Double space */
	case 055:	len = 3; break;	/* Triple space */
	case 061:	len = -1; break;/* Form feed */
	default:	len = 1;	/* Default -> single space */
            }
        }
    ptr = lfBuffer;
    *(ptr++) = CR;
    if (len == -1)
        *(ptr++) = FF;
    else if (len > 0)
        {
        while ((len--) > 0)
        *(ptr++) = LF;
        }
    len = ptr - lfBuffer;
    conn->fDataOutProc(conn->fDataOutRefCon, lfBuffer, len);
} /*ProcessCCTL*/

static int ProcessWriteRequest(tVTConnection * conn)
{ /*ProcessWriteRequest*/
    int returnValue = kVTCNoError;
    tVTMIORequest * writereq = (tVTMIORequest *) conn->fReceiveBuffer;
    tVTMTerminalIOResponse  * writeresp = (tVTMTerminalIOResponse * ) conn->fSendBuffer;
    char * writeData = writereq->fWriteData;
    uint16_t writeFlags = ntohs(writereq->fWriteFlags);
    uint16_t writeDataLength = ntohs(writereq->fWriteByteCount);
    extern unsigned char out_table[];
    extern int table_spec;
    
    if ((writeFlags & kVTIOWUseCCTL) &&
	(writeFlags & kVTIOWPrespace))
	{
#ifndef old_way
	ProcessCCTL(conn, ((writeDataLength) ? *writeData : '\0'));
#else
	if (writeDataLength)
	    ProcessCCTL(conn, *writeData);
	else
	    ProcessCCTL(conn, 0);
#endif
	}

    if (writeDataLength)
	{
/*
 * Do output translation here
 */
	if (table_spec)
	    {
	    int i = (writeFlags & kVTIOWUseCCTL) ? 1 : 0;
	    for (; i<writeDataLength; i++)
	      {
#if 0
		fprintf(stderr, "%c->%c", writeData[i], (char)out_table[((int)writeData[i]) & 0x00FF]);
#endif
		writeData[i] = (char)out_table[((int)writeData[i]) & 0x00FF];
	      }
	    }
	if (writeFlags & kVTIOWUseCCTL)
	    conn->fDataOutProc(conn->fDataOutRefCon, writeData + 1, (writeDataLength - 1));
	else
	    conn->fDataOutProc(conn->fDataOutRefCon, writeData, writeDataLength);
	}

    if ((writeFlags & kVTIOWUseCCTL) &&
	(!(writeFlags & kVTIOWPrespace)))
	{
/* Meaningless test.
	if (!(writeFlags & kVTIOWUseCCTL))
	    ProcessCCTL(conn, 0);
	else
 */
#ifndef old_way
	ProcessCCTL(conn, ((writeDataLength) ? *writeData : '\0'));
#else
	if (writeDataLength)
	    ProcessCCTL(conn, *writeData);
	else
	    ProcessCCTL(conn, 0);
#endif
	}

    if (writeFlags & kVTIOWNeedsResponse)
	{
	FillStandardMessageHeader((tVTMHeader *) writeresp,
				  kvmtTerminalIOResp, kVTIOWrite);
	writeresp->fRequestCount = writereq->fRequestCount;
	writeresp->fResponseCode = htons(kVTIOCSuccessful);
	writeresp->fCompletionMask = htons(kAMNRSuccessful);
	writeresp->fBytesRead = htons(0);
	returnValue = SendToAM(conn, (tVTMHeader *) writeresp,
			       sizeof(tVTMTerminalIOResponse) -
			       (sizeof(writeresp->fBytes)));
	}

    return returnValue;
} /*ProcessWriteRequest*/

static int ProcessReadRequest(tVTConnection * conn)
{ /*ProcessReadRequest*/
    int returnValue = kVTCNoError;
    tVTMIORequest * readreq = (tVTMIORequest *) conn->fReceiveBuffer;
    uint16_t readFlags = ntohs(readreq->fReadFlags);
    uint16_t readDataLength = ntohs(readreq->fReadByteCount);

    /* Set up for a read. Just save the parameters. */

    conn->fReadTimeout = ntohs(readreq->fTimeout);
    conn->fReadInProgress = true;
    conn->fReadStarted = true;	/* RM 960403 */
    if (readFlags & kVTIORFlushTypeAhead)
	conn->fReadFlush = true;	/* RM 960403 */
    conn->fReadLength = readDataLength;
    conn->fReadRequestCount = readreq->fRequestCount;
    conn->fEchoCRLFOnCR = true;
    if (readFlags & kVTIORNoCRLF) conn->fEchoCRLFOnCR = false;

    return returnValue;
} /*ProcessReadRequest*/

static int ProcessAbortRequest(tVTConnection * conn)
{ /*ProcessAbortRequest*/
    int returnValue = kVTCNoError;
    tVTMIORequest * abortreq = (tVTMIORequest *) conn->fReceiveBuffer;
    tVTMTerminalIOResponse  * resp = 
           (tVTMTerminalIOResponse * ) conn->fSendBuffer;

    FillStandardMessageHeader((tVTMHeader *) resp, kvmtTerminalIOResp,
				kVTIORead);
    resp->fRequestCount = abortreq->fReadFlags;
    resp->fResponseCode = htons(kVTIOCSuccessful);
    resp->fCompletionMask = htons(kVTIOCAborted);
    resp->fBytesRead = htons(0);
    returnValue = SendToAM(conn, (tVTMHeader *) resp,
			   sizeof(tVTMTerminalIOResponse) -
			   (sizeof(resp->fBytes)));
    if (returnValue != kVTCNoError)
	    goto Last;

    FillStandardMessageHeader((tVTMHeader *) resp, kvmtTerminalIOResp,
				kVTIOAbort);
    resp->fRequestCount = abortreq->fRequestCount;
    resp->fResponseCode = htons(kVTIOCSuccessful);
    returnValue = SendToAM(conn, (tVTMHeader *) resp,
			   sizeof(tVTMTerminalIOResponse) -
			   (sizeof(resp->fBytes)+
			    sizeof(resp->fBytesRead)+
			    sizeof(resp->fCompletionMask)));

 Last:
    return returnValue;
} /*ProcessAbortRequest*/

static int ProcessTerminalIOReq(tVTConnection * conn)
{ /*ProcessTerminalIOReq*/
    int returnValue = kVTCNoError;
    tVTMHeader * messageHeader = (tVTMHeader *) conn->fReceiveBuffer;
    
    switch (messageHeader->fPrimitive)
	{
    case kVTIORead:
	returnValue = ProcessReadRequest(conn);
	break;
	
    case kVTIOWrite:
	returnValue = ProcessWriteRequest(conn);
	break;

    case kVTIOAbort:
	returnValue = ProcessAbortRequest(conn);
	break;

        /* What do we do with read/write? */

    default:
	returnValue = kVTCReceivedInvalidIOPrimitive;
	conn->fLastSocketError = messageHeader->fPrimitive;
	break;
        }

    return returnValue;
} /*ProcessTerminalIOReq*/

static int ProcessEnvCntlResp(tVTConnection * conn)
{ /*ProcessEnvCntlResp*/
    int returnValue = kVTCNoError;
    tVTMHeader * messageHeader = (tVTMHeader *) conn->fReceiveBuffer;
    
    switch (messageHeader->fPrimitive)
	{
    case kvtpTMNegotiate:
	returnValue = ProcessTMNegotiationResponse(conn);
	break;

    case kvtpTerminate:
	returnValue = ProcessTerminationResponse(conn);
	break;

    case kvtpLogonInfo:
	/* We shouldn't get a sharing info response! */
	break;

    default:
	returnValue = kVTCReceivedInvalidControlPrimitive;
	conn->fLastSocketError = messageHeader->fPrimitive;
	break;
        }

    return returnValue;
} /*ProcessEnvCntlResp*/

static int ProcessTerminationRequest(tVTConnection * conn)
{ /*ProcessTerminationRequest*/
    int returnValue = kVTCNoError;
    tVTMTerminationRequest * termreq = (tVTMTerminationRequest *) 
						conn->fReceiveBuffer;
    tVTMTerminationResponse termResp;

    /* Send a termination response and tell the caller to shut down. */

    FillStandardMessageHeader((tVTMHeader *) &termResp, 
				kvmtEnvCntlResp, kvtpTerminate);
    termResp.fRequestCount = termreq->fRequestCount;
    termResp.fResponseCode = kvtRespNoError;
    SendToAM(conn, (tVTMHeader *) &termResp, sizeof(termResp));
    returnValue = kVTCStartShutdown;

    return returnValue;
} /*ProcessTerminationRequest*/

static int ProcessLogonInfo(tVTConnection * conn)
{ /*ProcessLogonInfo*/
    int returnValue = kVTCNoError;
    tVTMLogonInfo * termreq = (tVTMLogonInfo *) conn->fReceiveBuffer;
    tVTMLogonInfoResponse loginResp;

    /* What the heck am I supposed to do with this info? */
    /* Just send a response for now.                     */

    FillStandardMessageHeader((tVTMHeader *) &loginResp, 
				kvmtEnvCntlResp, kvtpLogonInfo);
    loginResp.fResponseCount = termreq->fRequestCount;
    loginResp.fResponseMask = htons(kvtRespFailed);
    returnValue = SendToAM(conn, (tVTMHeader *) &loginResp, sizeof(loginResp));

    return returnValue;
} /*ProcessLogonInfo*/

static int ProcessEnvCntlReq(tVTConnection * conn)
{ /*ProcessEnvCntlReq*/
    int returnValue = kVTCNoError;
    tVTMHeader * messageHeader = (tVTMHeader *) conn->fReceiveBuffer;
    
    switch (messageHeader->fPrimitive)
	{
    case kvtpAMNegotiate:
	returnValue = ProcessAMNegotiationRequest(conn);
	break;
	
    case kvtpTerminate:
	returnValue = ProcessTerminationRequest(conn);
	break;
	
    case kvtpLogonInfo:
	returnValue = ProcessLogonInfo(conn);
	break;
	
    default:
	returnValue = kVTCReceivedInvalidControlPrimitive;
	conn->fLastSocketError = messageHeader->fPrimitive;
	break;
        }

    return returnValue;
} /*ProcessEnvCntlReq*/

static int ProcessSetBreakRequest(tVTConnection * conn)
{ /*ProcessSetBreakRequest*/
    int returnValue = kVTCNoError;
    tVTMSetBreakRequest * breakreq = 
				(tVTMSetBreakRequest *) conn->fReceiveBuffer;
    tVTMSetBreakResponse breakResp;

    if (breakreq->fIndex == kVTSystemBreak)
	conn->fSysBreakEnabled = breakreq->fState;
    else if (breakreq->fIndex == kVTSubsysBreak)
	conn->fSubsysBreakEnabled = breakreq->fState;

    FillStandardMessageHeader((tVTMHeader *) &breakResp, 
				kvmtTerminalCntlResp, kvtpSetBreakInfo);
    breakResp.fRequestCount = breakreq->fRequestCount;
    breakResp.fResponseCode = htons(kvtRespFailed); /* ??? The spec must be wrong */
    returnValue = SendToAM(conn, (tVTMHeader *) &breakResp, sizeof(breakResp));

    return returnValue;
} /*ProcessSetBreakRequest*/

static int ProcessDriverControlRequest(tVTConnection * conn)
{ /*ProcessDriverControlRequest*/
    int returnValue = kVTCNoError;
    tVTMTerminalDriverControlRequest * req = 
		(tVTMTerminalDriverControlRequest *) conn->fReceiveBuffer;
    tVTMTerminalDriverControlResponse resp;
    uint16_t requestMask = ntohs(req->fRequestMask);
    uint16_t responseFlags = 0;
    bool	invalid_option = false;

    /* For each bit set in the request mask, set the corresponding	*/
    /* driver option. I don't think that any option needs immediate	*/
    /* action, though I suppose we could do some kind of notification	*/
    /* for any use of this package that doesn't use the internal	*/
    /* keyboard handling.						*/

    if (requestMask & kTDCMEcho)
	{
	conn->fEchoControl = req->fEcho;
	responseFlags |= kTDCMEcho;
	}

    /*
     * If this bit is set, fLineTermCharacter is the unedited eol
     */

    if (requestMask & kTDCMEditMode)
	{
	switch (req->fEditMode)
	    {
	case kDTCEditedMode:
	    conn->fUneditedMode = false;
	    conn->fLineTerminationChar = 015;	/* RM 960408 */
	    break;
	case kDTCUneditedMode:
	    conn->fUneditedMode = true;
	    conn->fLineTerminationChar = req->fLineTermCharacter;	/* RM 960408 */
	    break;
	case kDTCBinaryMode:
	    conn->fBinaryMode   = true;
	    break;
	case kDTCDisableBinary:
	    conn->fBinaryMode   = false;
	    break;
	default:  /* What to do? An error code? */ break;
	    }
	responseFlags |= kTDCMEditMode;
	}

    if (requestMask & kTDCMDriverMode)
	{
/* The reply is ignored by the host - so let's not send it!
	if (req->fDriverMode != kDTCVanilla)
	    responseFlags |= kDTCRDriverModeFailed;
 */
	if ((req->fDriverMode != kDTCVanilla) && (!(conn->fBlockModeSupported)))
	    {
	    invalid_option = true;
	    responseFlags |= kDTCRDriverModeFailed;
	    }
	else
	    {
	    conn->fDriverMode = req->fDriverMode;
	    responseFlags |= kTDCMDriverMode;
	    }
	}

/*
 * If this bit is set, fLineTermCharacter is the alternate eol
 */

    if (requestMask & kTDCMTermChar)
	conn->fAltLineTerminationChar = req->fLineTermCharacter;

    if (requestMask & kTDCMDataStream)
	{
	/* ??? What the heck is this? */
	/* In any case, let the AM know we're confused. (Well, the	*/
	/* programmer's confused, at least.)				*/

	responseFlags |= kDTCRStreamFailed;
	}

    if (requestMask & kTDCMEchoLine)
	{
	conn->fDisableLineDeleteEcho = (req->fEchoLineDelete == 0) ? true : false;
	responseFlags |= kTDCMEchoLine;
	}

    FillStandardMessageHeader((tVTMHeader *) &resp, 
				kvmtTerminalCntlResp, kvtpSetDriverInfo);
    resp.fRequestCount = req->fRequestCount;
    resp.fResponseCode = (invalid_option) ? htons(kVTIOCBadOp) : htons(kVTIOCSuccessful);
    resp.fStatusMask = htons(responseFlags);
    returnValue = SendToAM(conn, (tVTMHeader *) &resp, sizeof(resp));

    return returnValue;
} /*ProcessDriverControlRequest*/

static int ProcessTerminalCntlReq(tVTConnection * conn)
{ /*ProcessTerminalCntlReq*/
    int returnValue = kVTCNoError;
    tVTMHeader * messageHeader = (tVTMHeader *) conn->fReceiveBuffer;
    
    switch (messageHeader->fPrimitive)
	{
    case kvtpSetBreakInfo:
	returnValue = ProcessSetBreakRequest(conn);
	break;
	
    case kvtpSetDriverInfo:
	returnValue = ProcessDriverControlRequest(conn);
	break;
	
        /* What do we do with read/write and abort? */

    default:
	returnValue = kVTCReceivedInvalidIOPrimitive;
	conn->fLastSocketError = messageHeader->fPrimitive;
	break;
        }

    return returnValue;
} /*ProcessTerminalCntlReq*/

static int ProcessMPEControlReq(tVTConnection * conn)
{ /*ProcessMPEControlReq*/
    int returnValue = kVTCNoError;
    tVTMMPECntlReq * req = (tVTMMPECntlReq *) conn->fReceiveBuffer;
    tVTMMPECntlResp mpeResp;
    uint16_t requestMask = ntohs(req->fRequestMask);
    uint16_t completionMask = kDTCCSuccessful;

    if (requestMask & kDTCMSetTermType) conn->fTermType = req->fTermType;
    if (requestMask & kDTCMTypeAhead) conn->fTypeAhead = req->fTypeAhead;

    FillStandardMessageHeader((tVTMHeader *) &mpeResp, 
				kvmtMPECntlResp, kvtpMPECntl);	/* RM 960410 */
    mpeResp.fRequestCount = req->fRequestCount;
    mpeResp.fResponseCode = htons(kvtRespFailed);
    mpeResp.fCompletionMask = htons(completionMask);
    returnValue = SendToAM(conn, (tVTMHeader *) &mpeResp, sizeof(mpeResp));

    return returnValue;
} /*ProcessMPEControlReq*/

static int ProcessFDCControlReq(tVTConnection * conn)
{ /*ProcessFDCControlReq*/
    int returnValue = kVTCNoError;
    tVTMFDCCntlReq * req = (tVTMFDCCntlReq *) conn->fReceiveBuffer;
    tVTMFDCCntlResp fdcResp;

    /* What the heck am I supposed to do with this info? */
    /* Just send a response for now.                     */

    if (debug > 0)
	{
	char temp[32];
	sprintf(temp, "FDC func=%d, len=%d",
		htonl(req->fFDCFunc), htons(req->fFDCLength));
	DumpBuffer(req->fFDCBuffer, htons(req->fFDCLength), temp);
	}
    FillStandardMessageHeader((tVTMHeader *) &fdcResp, 
				kvmtGenericFDCResp, kvtpDevSet);	/* RM 960410 */
    fdcResp.fRequestCount = req->fRequestCount;
    memcpy(fdcResp.fFDCBuffer, req->fFDCBuffer, sizeof(req->fFDCBuffer));
    fdcResp.fFDCFunc = htonl(req->fFDCFunc);
    fdcResp.fFDCLength = htons(req->fFDCLength);
    fdcResp.fFDCErrorCode = htons(kvtRespNoError);
    returnValue = SendToAM(conn, (tVTMHeader *) &fdcResp, sizeof(fdcResp));

    return returnValue;
} /*ProcessFDCControlReq*/

static int GenerateApplControlReq(tVTConnection * conn, int send_index)
{ /*GenerateApplControlReq*/
    int returnValue = kVTCNoError;
    tVTMApplCntlResp ApplResp;

    FillStandardMessageHeader((tVTMHeader *) &ApplResp, 
				kvmtApplicationCntlReq, kvtpApplInvokeBreak);
    ApplResp.fUnused = 0xFF;
    ++appl_req_count;
    ApplResp.fRequestCount = htons(appl_req_count);
    ApplResp.fApplIndex = htons(send_index);
    returnValue = SendToAM(conn, (tVTMHeader *) &ApplResp, sizeof(ApplResp));

    return returnValue;
} /*GenerateApplControlReq*/

static int ProcessReceivedRecord(tVTConnection * conn)
{ /*ProcessReceivedRecord*/
    int returnValue = kVTCNoError;
    tVTMHeader * messageHeader = (tVTMHeader *) conn->fReceiveBuffer;

    messageHeader->fMessageLength = ntohs(messageHeader->fMessageLength);

    if (messageHeader->fProtocolID != kVTProtocolID)
	{
        returnValue = kVTCReceivedInvalidProtocolID;
	goto Last;
	}
    
    switch (messageHeader->fMessageType)
	{
    case kvmtEnvCntlReq:
	returnValue = ProcessEnvCntlReq(conn);
	break;
	
    case kvmtEnvCntlResp:
	returnValue = ProcessEnvCntlResp(conn);
	break;
	
    case kvmtTerminalIOReq:
	returnValue = ProcessTerminalIOReq(conn);
	break;
	
    case kvmtTerminalIOResp:
	/* The TM isn't supposed to get these. Report an error if   */
	/* we get one.						*/
	returnValue = kVTCReceivedUnexpectedIOResp;
	break;
	
    case kvmtTerminalCntlReq:
	returnValue = ProcessTerminalCntlReq(conn);
	break;
	
    case kvmtTerminalCntlResp:
	/* The TM isn't supposed to get these. Report an error if	*/
	/* someone sends us one.					*/
	returnValue = kVTCReceivedUnexpectedControlResp;
	break;
	
    case kvmtApplicationCntlReq:
	/* The TM isn't supposed to get these. Report an error if	*/
	/* someone sends us one.					*/
	returnValue = kVTCReceivedUnexpectedAppCtlReq;
	break;
	
    case kvmtApplicationCntlResp:
	/* ??? The docs say there isn't one of these, but I can't   */
	/* figure out why there wouldn't be.                        */
	/* This is a response to an ApplicationCntlReq generated by */
	/*   us.  RM 960409 */
	break;

    case kvmtMPECntlReq:
	/* No docs on this... we do get these requests so for the	*/
	/* moment we just piece together a generic reply.		*/
	returnValue = ProcessMPEControlReq(conn);
	break;

    case kvmtMPECntlResp:
	/* I'm pretty sure we aren't supposed to get these :-)	*/
	returnValue = kVTCReceivedInvalidMessageType;
	conn->fLastSocketError = messageHeader->fMessageType;
	break;
	
    case kvmtGenericFDCReq:
	/* No docs on this... we do get these requests so for the	*/
	/* moment we just piece together a generic reply.		*/
	returnValue = ProcessFDCControlReq(conn);
	break;

    case kvmtGenericFDCResp:
	/* I'm pretty sure we aren't supposed to get these :-)	*/
	returnValue = kVTCReceivedInvalidMessageType;
	conn->fLastSocketError = messageHeader->fMessageType;
	break;

    default:
	returnValue = kVTCReceivedInvalidMessageType;
	conn->fLastSocketError = messageHeader->fMessageType;
	}

Last:
    return returnValue;
} /*ProcessReceivedRecord*/

void VTErrorMessage(tVTConnection * conn, int errorCode, char * msg, int maxLength)
{ /*VTErrorMessage*/
    char  messageBuffer[512];  /* Plenty long enough */

    messageBuffer[0] = 0;	/* Initialize to nothing */

    switch (errorCode)
	{
    case kVTCNoError: 
	strcpy(messageBuffer, "Successful execution; no error.");
	break;

    case kVTCMemoryAllocationError:
	strcpy(messageBuffer, "Unable to allocate requested memory.");
	break;

    case kVTCSocketError:
	if (conn)
	    sprintf(messageBuffer, "Socket error: %s.", 
		    strerror(conn->fLastSocketError));
	else strcpy(messageBuffer, "Socket error.");
	break;

    case kVTCCantAllocateSocket:
	strcpy(messageBuffer, "Unable to allocate a socket.");
	break;

    case kVTCNotInitialized:
	strcpy(messageBuffer, "Attempted open of uninitialized connection.");
	break;

    case kVTCReceiveRecordLengthError:
	strcpy(messageBuffer, "Received invalid length for VT request.");
	break;

    case kVTCReceivedInvalidProtocolID:
	strcpy(messageBuffer, "Received an invalid protocol ID.");
	break;

    case kVTCReceivedInvalidMessageType:
	if (conn)
	    sprintf(messageBuffer, "Received an invalid message type (%d).",
		    conn->fLastSocketError);
	else strcpy(messageBuffer, "Received an invalid message type.");
	break;

    case kVTCReceivedInvalidControlPrimitive:
	if (conn)
	    sprintf(messageBuffer, "Received an invalid control primitive (%d).",
		    conn->fLastSocketError);
	else strcpy(messageBuffer, "Received an invalid control primitive.");
	break;

    case kVTCSendError:
	if (conn)
	    sprintf(messageBuffer, "Socket error on send: %s.", 
		    strerror(conn->fLastSocketError));
	else strcpy(messageBuffer, "Socket error on send.");
	break;

    case kVTCRejectedTMNegotiation:
	strcpy(messageBuffer, "Error: AM rejected TM negotiation.");
	break;

    case kVTCVTOpen:
	strcpy(messageBuffer, "Connection status: connection is open.");
	break;

    case kVTCStartShutdown:
	strcpy(messageBuffer, "Received shutdown request from AM.");
	break;

    case kVTCReceivedInvalidIOPrimitive:
	strcpy(messageBuffer, "Received invalid I/O primitive.");
	break;

    case kVTCReceivedUnexpectedIOResp:
	strcpy(messageBuffer, "Received unexpected I/O response.");
	break;

    case kVTCReceivedUnexpectedControlResp:
	strcpy(messageBuffer, "Received unexpected control response.");
	break;

    case kVTCReceivedUnexpectedAppCtlReq:
	strcpy(messageBuffer, "Received unexpected app control request.");
	break;

    default:
	sprintf(messageBuffer, "Unknown socket error code %d.", errorCode);
	break;
	}

    if ((int)strlen(messageBuffer) > maxLength - 1)
	{
	memcpy(msg, messageBuffer, maxLength - 1);
	msg[maxLength - 1] = 0;
	}
    else strcpy(msg, messageBuffer);
} /*VTErrorMessage*/

int VTInitConnection(tVTConnection * conn, long ipAddress, int ipPort)
{ /*VTInitConnection*/
    int returnValue = kVTCNoError;	/* Assume failure.	*/

    /* Attempt to allocate the receive and transmit buffers.    */
    /* The receive buffer, at least, must be allocated before   */
    /* we attempt a connection since the first thing that       */
    /* should happen is that the AM will tell us how big the    */
    /* buffer should be.					*/

    returnValue = kVTCMemoryAllocationError;

    conn->fSendBuffer = (char *) malloc(kVT_MAX_BUFFER);
    if (conn->fSendBuffer == NULL) goto Last;

    conn->fReceiveBuffer = (char *) malloc(kVT_MAX_BUFFER);
    if (conn->fReceiveBuffer == NULL) 
        {
        free(conn->fSendBuffer);
        goto Last;
        }

    /* There are a few things that the AM never tells us but 	*/
    /* that it's pretty clear we're suppsed to know. One of     */
    /* these is the default EOR character. Are there any        */
    /* others?							*/

    conn->fLineTerminationChar = 015;  /* CR */

    conn->fSendBufferSize = kVT_MAX_BUFFER;
    conn->fReceiveBufferSize = kVT_MAX_BUFFER;

    conn->fTargetAddress.sin_family = AF_INET;
    conn->fTargetAddress.sin_port = htons(ipPort);
    conn->fTargetAddress.sin_addr.s_addr = ipAddress;

    /* Now, allocate a socket. */

    conn->fSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->fSocket == -1)
	{
	returnValue = kVTCCantAllocateSocket;
	goto Last;
	}

    conn->fDataOutProc = DefaultDataOutProc;
    conn->fState = kvtsClosed;
    conn->fDriverMode = kDTCVanilla;
    conn->fBlockModeSupported = false;	/* RM 960411 */
    returnValue = kVTCNoError;

Last:
    return returnValue;
} /*VTInitConnection*/

void VTCleanUpConnection(tVTConnection * conn)
{ /*VTCleanUpConnection*/
    if (conn->fSendBuffer) free(conn->fSendBuffer);
    if (conn->fReceiveBuffer) free(conn->fReceiveBuffer);
    if (conn->fSocket != -1) 
	{
	shutdown(conn->fSocket, 2);
        close(conn->fSocket);
        }

    memset((char *) conn, 0, sizeof(*conn));
    conn->fSocket = -1;
} /*VTCleanUpConnection*/

int VTSocket(tVTConnection * conn)
{ /*VTSocket*/
    return conn->fSocket;
} /*VTSocket*/

int VTConnect(tVTConnection * conn)
{ /*VTConnect*/
    int  connectError;
    int  returnValue = kVTCNoError;	/* Assume success */
    int
	flags = 0;
#ifdef IPPROTO_IP
#ifdef IP_TOS
    int tos = 0;
#endif /* IP_TOS */
#endif /* IPPROTO_IP */
#ifdef IPPROTO_TCP
#ifdef TCP_NOOPT
    int noopt = 1;
#endif /* TCP_NOOPT */
#endif /* IPPROTO_TCP */

    if (conn->fState != kvtsClosed)
	{
	returnValue = kVTCNotInitialized;
	goto Last;
	}

#ifdef IPPROTO_IP
#ifdef IP_TOS
    /* older versions of NS Transport on classic MPE V/E (V-delta-9, before, likely
     * some after) have trouble with IP type of service (TOS) header field being non-zero;
     * and older versions of NS/3000 VT server will call SUDDENDEATH(969) on the resulting
     * socket error; so force it to zero and don't be the remote denial-of-service
     */
    if (0 > setsockopt(conn->fSocket, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)))
	{
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
	goto Last;
	}
#endif /* IP_TOS */
#endif /* IPPROTO_IP */

#ifdef IPPROTO_TCP
#ifdef TCP_NOOPT
    /* older versions of NS Transport on classic MPE V/E (V-delta-9, before, likely
     * some after) also have trouble with some TCP options, in particular Selective
     * Acknowledgement (SACK) and Window Scaling.  Window Scaling seems to make TCP 
     * ignore the SYN segment until it is retransmitted with the Window Scaling option
     * removed.  SACK is accepted but provokes a call to SUDDENDEATH(969), so we 
     * socket error; so force it to zero and don't be the remote denial-of-service, 
     * and as a bonus get connected faster, but too bad about MSS negotiation via
     * TCP options.
     */
    if (0 > setsockopt(conn->fSocket, IPPROTO_TCP, TCP_NOOPT, &noopt, sizeof(noopt)))
        {
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
	goto Last;
	}
#endif /* TCP_NOOPT */
#endif /* IPPROTO_TCP */

    connectError = connect(conn->fSocket, 
			   (struct sockaddr *) &conn->fTargetAddress,
			   sizeof(conn->fTargetAddress));

    if (connectError)
	{
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
	goto Last;
	}

#  ifdef O_NONBLOCK
    if ((flags = fcntl(conn->fSocket, F_GETFL, 0)) == -1)
	{
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
	goto Last;
	}
    flags |= O_NONBLOCK;
    if (fcntl(conn->fSocket, F_SETFL, flags) == -1)
	{
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
	goto Last;
	}
#  endif

    SetUpForNewRecordReceive(conn);
    conn->fState = kvtsWaitingForAM;

Last:
    return returnValue;
} /*VTConnect*/

int VTReceiveDataReady(tVTConnection * conn)
{ /*VTReceiveDataReady*/
    int    returnValue = kVTCNoError;
    char * dataDest;
    size_t lengthToReceive;
    ssize_t
	   receivedLength;

    if (conn->fReceiveBufferOffset < 0)  /* Receiving length word? */
        {
        dataDest = conn->fReceiveBufferOffset +
			(char *) &conn->fLengthWord + 
			    sizeof(conn->fLengthWord);
        lengthToReceive = -conn->fReceiveBufferOffset;
        }
    else   /* Receiving data, not length */
	{
	dataDest = conn->fReceiveBuffer +
			conn->fReceiveBufferOffset;
        lengthToReceive = conn->fLengthToReceive;
        }

    receivedLength = read(conn->fSocket, dataDest, lengthToReceive);
    if (receivedLength > 0)
        {
        if (conn->fReceiveBufferOffset < 0) /* Data intended for len?*/
	    {  /* Yes, accumulate in length word */
            conn->fReceiveBufferOffset += receivedLength;
            if (conn->fReceiveBufferOffset >= 0)  
                {
                /* We have the length word. Set up to receive the record. */

                assert(conn->fReceiveBufferOffset == 0);
                conn->fLengthToReceive = ntohs(conn->fLengthWord) - 2;

		/* Validate the received length word. */

		if ((conn->fLengthToReceive > conn->fReceiveBufferSize - 2) ||
		    (conn->fLengthToReceive < 2))
		    {
		    returnValue = kVTCReceiveRecordLengthError;
	    	    goto Last;
		    }

		/* Copy the length word into the buffer, then set the	*/
		/* offset to point past it. The remainder of the record */
		/* will follow the length word.				*/

		memcpy(conn->fReceiveBuffer,
			(char *) &conn->fLengthWord, 2);
                conn->fReceiveBufferOffset = 2; /* Past length word */
		}
            }
        else  /* A record receive, not a data receive */
            {
	    assert(receivedLength <= conn->fLengthToReceive);
	    conn->fReceiveBufferOffset += receivedLength;
	    conn->fLengthToReceive -= receivedLength;
	    if (conn->fLengthToReceive <= 0)
		{
		if (debug > 0)
		    DumpBuffer(dataDest, receivedLength, "from_host");
		returnValue = ProcessReceivedRecord(conn);
		SetUpForNewRecordReceive(conn);
		}
            }
        }
    else if (receivedLength < 0)  /* Error occurred? */
        {
	returnValue = kVTCSocketError;
	conn->fLastSocketError = errno;
        }

Last:
    return returnValue;
} /*VTReceiveDataReady*/

int VTSendBreak(tVTConnection * conn, int send_index)
{ /*VTSendBreak*/
    return(GenerateApplControlReq(conn, send_index));
} /*VTSendBreak*/

int VTSendData(tVTConnection * conn, char * buffer, int length, int comp_mask)
{ /*VTSendData*/
    int   	returnValue = kVTCNoError;
    tVTMTerminalIOResponse  * resp = 
           (tVTMTerminalIOResponse * ) conn->fSendBuffer;

    if (length > conn->fSendBufferSize - sizeof(tVTMTerminalIOResponse)) 
             length = conn->fSendBufferSize - sizeof(tVTMTerminalIOResponse);

    FillStandardMessageHeader((tVTMHeader *) resp, kvmtTerminalIOResp,
				kVTIORead);
    resp->fResponseCode = ((comp_mask == kVTIOCSuccessful)
			   ? htons(kVTIOCSuccessful)
			   : htons(kVTIOCEOF));
    resp->fCompletionMask = htons(comp_mask);
    resp->fBytesRead = htons(length);
    resp->fRequestCount = conn->fReadRequestCount;
    memcpy(resp->fBytes, buffer, length);
    returnValue = SendToAM(conn, (tVTMHeader *) resp,
        length + sizeof(tVTMTerminalIOResponse) - sizeof(resp->fBytes));
/*
Last:
 */
    return returnValue;
} /*VTSendData*/

/* Local Variables: */
/* c-indent-level: 0 */
/* c-continued-statement-offset: 4 */
/* c-brace-offset: 0 */
/* c-argdecl-indent: 4 */
/* c-label-offset: -4 */
/* End: */
