#ifndef drvAiSecond_h
#define drvAiSecond_h

#endif

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include <drvSup.h>
#include "aiRecord.h"
#include "link.h"
#include "epicsExport.h"

#include <dbAccess.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTimer.h>
#include <epicsEvent.h>
#include <epicsExport.h>

#include <aoRecord.h>

#include <unistd.h>
#include <poll.h>
#include <termios.h>
#define CRLF "\r\n"
#define Sleep(x) usleep(x*1000)

#include "pniousrx.h"
#include "pnioerrx.h"
#include "pniobase.h"

long CP_IOC_Input();
long CP_IOC_Output();