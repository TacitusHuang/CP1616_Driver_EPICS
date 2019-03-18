#include "drvAiSecond.h"
/*------------------------------------------------------------------------*/
/* Global Data for Communication Processor                                */
/*------------------------------------------------------------------------*/

/* Index of CP given by project */
PNIO_UINT32 g_dwCpId = 1;


/* Don't forget! Callbacks are running in concurrent threads !            */
/* 'volatile PNIO_MODE_TYPE g_currentMode' statement is                   */
/* required to force the compiler to generate unoptimized memory access   */
/* to 'g_currentMode'.                                                    */
/*                                                                        */
/* Example                                                                */
/*  while(g_currentMode == PNIO_MODE_OFFLINE){...};                       */
/* will not be optimized                                                  */
volatile PNIO_MODE_TYPE g_currentMode    = PNIO_MODE_OFFLINE;


/* Global variables to count callback events */
volatile PNIO_UINT32 g_OpFaultCount      = 0;
volatile PNIO_UINT32 g_StartOpCount      = 0;
volatile PNIO_UINT32 g_DataExchangeCount = 0;

/* Global variables to control the behavoir of the DataExchange function */
volatile int g_data                      = 0;
volatile int g_set                      = 0;
volatile int g_mirror                   = 0;
volatile int g_increment                = 0;
void (*g_DataExchange) (PNIO_UINT32)     = 0;

volatile PNIO_UINT32 g_readErrors        = 0;
volatile PNIO_UINT32 g_writeErrors       = 0;
volatile PNIO_UINT32 g_badRemoteStatus   = 0;

/*------------------------------------------------------------------------*/
/* Global Data for Device                                                 */
/*------------------------------------------------------------------------*/

volatile PNIO_IOXS g_localState = PNIO_S_GOOD;

typedef struct {
    PNIO_ADDR address;
    PNIO_UINT32 length;
    volatile PNIO_IOXS status;
    volatile PNIO_UINT8 *data;
} device;

device g_InputDevice[] = {
    /* address                        length rem status  data */
    {{PNIO_ADDR_LOG, PNIO_IO_IN, 4}, 1, PNIO_S_BAD, NULL},
    {{PNIO_ADDR_LOG, PNIO_IO_IN, 5}, 4, PNIO_S_BAD, NULL},
    {{PNIO_ADDR_LOG, PNIO_IO_IN, 9}, 8, PNIO_S_BAD, NULL},
};

const PNIO_UINT32 g_deviceInputCount = sizeof (g_InputDevice) / sizeof (device);        // number of input modules

device g_OutputDevice[] = {
    /* address                         length rem status  data */
    {{PNIO_ADDR_LOG, PNIO_IO_OUT,    2},   2, PNIO_S_BAD, NULL},
    {{PNIO_ADDR_LOG, PNIO_IO_OUT,    8},   4, PNIO_S_BAD, NULL}
};

 const PNIO_UINT32 g_deviceOutputCount = sizeof (g_OutputDevice) / sizeof (device);      // number of output modules

/*======================================================================================================*/


/*------------------------------------------------------------------------*/
/* mandatory callbacks                                                    */
/* but not used in this sample application                                */
/* because this example does no data set, read or write                   */
/*------------------------------------------------------------------------*/

void callback_for_ds_read_conf(PNIO_CBE_PRM * pCbfPrm);
void callback_for_ds_write_conf(PNIO_CBE_PRM * pCbfPrm);


/*------------------------------------------------------------------------*/
/* optional callbacks                                                     */
/*------------------------------------------------------------------------*/

void callback_for_mode_change_indication(PNIO_CBE_PRM * pCbfPrm);
void callback_for_alarm_indication(PNIO_CBE_PRM * pCbfPrm);


/*------------------------------------------------------------------------*/
/* callbacks for IRT function                                             */
/*------------------------------------------------------------------------*/

void callback_for_startop_indication(PNIO_CP_CBE_PRM * prm);
void callback_for_opfault_indication(PNIO_CP_CBE_PRM * prm);


/*------------------------------------------------------------------------*/
/* forward declaration of helper functions                                */
/*------------------------------------------------------------------------*/

PNIO_UINT32 Initialize(PNIO_UINT32 CP_INDEX);
void UnInitialize(PNIO_UINT32 dwHandle);
void DataExchange(PNIO_UINT32 dwHandle);
void UpdateCyclicOutputData(PNIO_UINT32 dwHandle);
void UpdateCyclicInputData(PNIO_UINT32 dwHandle);
int getCharWithTimeout(void);
void ChangeAndWaitForPnioMode(PNIO_UINT32 dwHandle, PNIO_MODE_TYPE mode);

/*------------------------------------------------------------------------*/


/*******************************************************************/
/* DataExchange                                                    */
/*******************************************************************/
/* This function is called in the StartOP handler                  */
/*******************************************************************/


static long cp1616IoReport();
static long cp1616Init();

int cp1616main_irt ();


struct {
    long number;
    long (*report)();
    long (*init)();
} cp1616 = {
    2,
    cp1616IoReport,
    cp1616Init
};

epicsExportAddress(drvet, cp1616);

static long cp1616IoReport(int after)
  {
    return(0);
  }


 static long cp1616Init(int after)
  {
    printf("CP1616_Test_Init\n");
    epicsThreadCreate(
        "cp1616main_irt",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC)cp1616main_irt,
        NULL);
    return 0;
  }



    long CP_IOC_Input()
    {
    int ioc_input =8;
    ioc_input =* g_InputDevice[0].data;
    return ioc_input;
    }
    
    long CP_IOC_Output(int ioc_Output)
    {
    *g_OutputDevice[0].data=ioc_Output;
    }


/*******************************************************************/
/* IRT Start OP                                                    */
/*******************************************************************/
/* This callback is called at the start of IRT cycle               */
/*******************************************************************/
void callback_for_startop_indication(PNIO_CP_CBE_PRM * prm)
{
    ++g_StartOpCount;

    if(g_DataExchange) {
        /* process data */
        g_DataExchange(prm->u.StartOp.AppHandle);
    }

    /* signal that data processing is done for this cycle */
    PNIO_CP_set_opdone(prm->u.StartOp.AppHandle, NULL);
}


/************************************************************************/
/* IRT OPFault_ind                                                      */
/************************************************************************/
/* This callback is called if OPdone is not called in time of IRT cycle */
/************************************************************************/
void callback_for_opfault_indication(PNIO_CP_CBE_PRM * prm)
{
    /* Opfault has come */
    ++g_OpFaultCount;
}


/*-------------------------------------------------------------*/
/* this function sets operational mode                         */
/*                                                             */
/* do not call before Initialize was called successfully       */
/* because it needs PNIO_CBE_MODE_IND callback to be registered*/
/*-------------------------------------------------------------*/
void ChangeAndWaitForPnioMode(PNIO_UINT32 dwHandle, PNIO_MODE_TYPE mode)
{
    PNIO_UINT32 dwErrorCode;

    /* setting  mode asynchronously */
    dwErrorCode = PNIO_set_mode(dwHandle, mode);

    if(dwErrorCode != PNIO_OK) {
        printf(" Error in ChangeAndWaitForPnioMode\n");
        printf(" PNIO_set_mode returned 0x%x\n", dwErrorCode);
        PNIO_close(dwHandle);
        exit(1);                // exit
    }
    /* wait for callback_for_mode_change_indication to be called. */
    /* callback_for_mode_change_indication sets g_currentMode     */
    printf("waiting for changing operation mode\n");
    while(g_currentMode != mode) {
        Sleep(100);
        printf(".");
        fflush(stdout);
    }

    printf(CRLF);
}

/*------------------------------------------------------------------------*/
/* Here starts the application code                                       */
/*------------------------------------------------------------------------*/
int cp1616main_irt(int argc, char *argv[])
{
    PNIO_UINT32 dwHandle = 0;
    int key = 0;

    dwHandle = Initialize(g_dwCpId);
    printf("Initialize successful\n");
    printf("g_deviceOutputCount is %d\n",g_deviceOutputCount);
    printf("g_deviceInputCount is %d\n",g_deviceInputCount);  
    // TODO wait until all devices are online

    do
    {
    ++g_DataExchangeCount;
    UpdateCyclicInputData(dwHandle);

    if(g_OutputDevice[1].length == sizeof (PNIO_UINT32))
     {
        ++  *g_OutputDevice[1].data;
     }
    else 
     {
        printf("some errors happen\n");
     }


	if (*g_InputDevice[0].data == 15)
	 {*g_OutputDevice[0].data = 1;
	}
	else
	{        
	*g_OutputDevice[0].data = 0;
       }
	
    UpdateCyclicOutputData(dwHandle);
    Sleep(2);
    } while (g_DataExchangeCount <= 10000);


    UnInitialize(dwHandle);
    printf("UnInitialize Successfully\n");

    printf("Statistics: %8u StartOp callbacks, %8u DataExchange calls, %8u OpFault callbacks\n",
            g_StartOpCount, g_DataExchangeCount, g_OpFaultCount);
    printf("            %8u Data write errors, %8u Data read errors,   %8u Bad remote status\n",
            g_writeErrors, g_readErrors, g_badRemoteStatus);

    return 0;
}


/*-------------------------------------------------------------*/
/* this function initializes the IO-BASE and returns a handle  */
/* necessary for all subsequent calls to IO-BASE functions     */
/*-------------------------------------------------------------*/
PNIO_UINT32 Initialize(PNIO_UINT32 CP_INDEX)
{
    PNIO_UINT32 dwHandle = 0;   /* 0 is invalid handle */
    PNIO_UINT32 dwErrorCode = PNIO_OK;

    /* allocate memory for Input data */    
    printf("\nInitialization START!\n");
    int i = 0;
    do
    {
         g_InputDevice[i].data = (uint8_t*)malloc(g_InputDevice[i].length);
         printf("g_InputDevice[%d].data is %d\n",i,*g_InputDevice[i].data);
         int sizeIn = sizeof(*g_InputDevice[i].data);
         printf("Size of g_InputDevice[%d].data is %d\n\n",i,sizeIn);        
         if(!g_InputDevice[i].data) 
            {
             printf("Could not allocate memory (%d bytes) for g_InputData[%d]\n",
                     g_InputDevice[i].length, i);
             } 
             else 
             {
             memset((void *)g_InputDevice[i].data, 0, g_InputDevice[i].length);
             }
         ++i;
    } while (i < g_deviceInputCount);


    /* allocate memory for output data */
    int j =0 ;
    do
    {
        g_OutputDevice[j].data = (uint8_t*)malloc(g_OutputDevice[j].length);

         g_OutputDevice[j].data = (uint8_t*)malloc(g_OutputDevice[j].length);
         printf("g_OutputDevice[%d].data is %d\n",j,*g_InputDevice[j].data);
         int sizeOut = sizeof(g_InputDevice[j].data);
          printf("Size of g_OutputDevice[%d].data is %d\n\n",j,sizeOut); 
         if(!g_OutputDevice[j].data) 
            {
            printf("Could not allocate memory (%d bytes) for g_OutputData[%d]\n",
                    g_OutputDevice[j].length, j);
            } 
            else 
            {
            memset((void *)g_OutputDevice[j].data, 0, g_OutputDevice[j].length);}
         j++;
    } while (j < g_deviceOutputCount);


    /* Connect to Communication Processor and obtain a handle */
        dwErrorCode = PNIO_controller_open(
            /*in*/  CP_INDEX,                     /* index of communication processor      */
            /*in*/  PNIO_CEP_MODE_CTRL,           /* permission to change operation mode   */
            /*in*/  callback_for_ds_read_conf,    /* mandatory  callback                   */
            /*in*/  callback_for_ds_write_conf,   /* mandatory callback                    */
            /*in*/  callback_for_alarm_indication,/* alarm callback                        */
            /*out*/ &dwHandle);                   /* handle                                */

    if(dwErrorCode != PNIO_OK) {
        printf("Error in Initialize\n");
        printf("PNIO_controller_open returned 0x%08x\n", dwErrorCode);
        exit(1);                // exit
    }

    /*Register the IRT callbacks */
    dwErrorCode = PNIO_CP_register_cbf(dwHandle, PNIO_CP_CBE_OPFAULT_IND, callback_for_opfault_indication);

    if(dwErrorCode != PNIO_OK) {
        /* Error */
        printf("\n\t Error while registering OpFault callback function , check if you are logged in as root\n");
        PNIO_close(dwHandle);
        exit(1);                // exit
    }

    dwErrorCode = PNIO_CP_register_cbf(dwHandle, PNIO_CP_CBE_STARTOP_IND, callback_for_startop_indication);
    if(dwErrorCode != PNIO_OK) {
        /* Error */
        printf("\n\t Error while registering StartOP callback function , check if you are logged in as root\n");
        PNIO_close(dwHandle);
        exit(1);                // exit
    }

    /* here we register the callback                         */
    /* PNIO_CBE_MODE_IND    for Mode changes  confirmation   */
    dwErrorCode = PNIO_register_cbf(
            /*in */ dwHandle,
            /*in */ PNIO_CBE_MODE_IND,
            /*in */ callback_for_mode_change_indication);

    if(dwErrorCode != PNIO_OK) {
        printf(" Error in Initialize\n");
        printf(" PNIO_register_cbf (PNIO_CBE_MODE_IND,..)  returned 0x%08x\n", dwErrorCode);
        PNIO_close(dwHandle);
        exit(1);                // exit
    }

    /* here we change the mode to PNIO_MODE_OPERATE  */
    ChangeAndWaitForPnioMode(dwHandle, PNIO_MODE_OPERATE);
    printf("\nnitialization END!\n\n\n\n");
    return dwHandle;
}

/*-------------------------------------------------------------*/
/* this function uninitializes the IO-BASE                     */
/*                                                             */
/* parameters                                                  */
/*    handle         : handle to the communication processor   */
/*-------------------------------------------------------------*/
void UnInitialize(PNIO_UINT32 dwHandle)
{
    PNIO_UINT32 dwErrorCode = PNIO_OK;
    /* here we change the mode to PNIO_MODE_OFFLINE  */
    ChangeAndWaitForPnioMode(dwHandle,PNIO_MODE_OFFLINE);

    dwErrorCode = PNIO_close(dwHandle);

    if(dwErrorCode != PNIO_OK) {
        printf("Error in UnInitialize\n");
        printf("PNIO_close returned 0x%08x\n", dwErrorCode);
        exit(1);                // exit
    }

      /* free memory for input data */
    int i =0;
    do
    {
          if(g_InputDevice[i].data) {
              free(g_InputDevice[i].data);
              g_InputDevice[i].data = NULL;
          }
      ++i;    
    } while (i < g_deviceInputCount);

      /* free memory for output data */
    int j =0;
    do
    {
          if(g_OutputDevice[j].data) {
              free(g_OutputDevice[j].data);
              g_OutputDevice[j].data = NULL;
          }
      ++j;    
    } while (j < g_deviceOutputCount);   
}

/*-------------------------------------------------------------*/
/* this function writes the cyclic output data to IO-BASE      */
/*                                                             */
/* parameters                                                  */
/*    handle         : handle to the communication processor   */
/*-------------------------------------------------------------*/
void UpdateCyclicOutputData(PNIO_UINT32 dwHandle)
{
    PNIO_UINT32 dwErrorCode;
    int i =0 ;
    do
    {
        dwErrorCode = PNIO_data_write(
                /*in*/ dwHandle,                   /*handle                            */
                /*in*/ &g_OutputDevice[i].address, /* pointer to device output address */
                /*in*/ g_OutputDevice[i].length,   /* length in bytes of output        */
                /*in*/ (PNIO_UINT8*)g_OutputDevice[i].data, /* pointer to output data  */
                /*in*/ g_localState,               /* local status                     */
                /*out*/(PNIO_IOXS*)&g_OutputDevice[i].status); /* remote status        */

        if(dwErrorCode != PNIO_OK)
            ++g_writeErrors;
        else if(g_OutputDevice[i].status == PNIO_S_BAD)
            ++g_badRemoteStatus;
    i++;

    } while (i < g_deviceOutputCount);

}

/*-------------------------------------------------------------*/
/* this function reads the cyclic input data from IO-BASE      */
/*                                                             */
/* parameters                                                  */
/*    handle         : handle to the communication processor   */
/*-------------------------------------------------------------*/
void UpdateCyclicInputData(PNIO_UINT32 dwHandle)
{
    PNIO_UINT32 dwErrorCode;
    PNIO_UINT32 dwBytesReaded;
    int i =0 ;    
    do
    {
        dwErrorCode = PNIO_data_read(
                /*in*/  dwHandle,                 /*handle                           */
                /*in*/  &g_InputDevice[i].address,/* pointer to device input address */
                /*in*/  g_InputDevice[i].length,  /* length in bytes of input        */
                /*out*/ &dwBytesReaded,           /* number of bytes read            */
                /*in*/  (PNIO_UINT8*)g_InputDevice[i].data, /* pointer to input data */
                /*in*/  g_localState,             /* local status                    */
                /*out*/ (PNIO_IOXS*)&g_InputDevice[i].status);/* remote status       */

        if(dwErrorCode != PNIO_OK)
            ++g_readErrors;
        else if(g_InputDevice[i].status == PNIO_S_BAD)
            ++g_badRemoteStatus;
        i ++;
    } while (i < g_deviceInputCount);

}

/*--------------------------------------------------*/
/* mandatory callbacks but not used in this sample  */
/*--------------------------------------------------*/
void callback_for_ds_read_conf(PNIO_CBE_PRM * pCbfPrm)
{
    /***************************************************************/
    /* Attention :                                                 */
    /* this is a callback and must be returned as soon as possible */
    /* don't use any endless or time consuming functions           */
    /* e.g. exit() would be fatal                                  */
    /* defer all time consuming functionality to other threads     */
    /***************************************************************/

    printf("callback_for_ds_read_conf\n");
    printf("this callback must not occur in this sample application\n");
}

void callback_for_ds_write_conf(PNIO_CBE_PRM * pCbfPrm)
{
    /***************************************************************/
    /* Attention :                                                 */
    /* this is a callback and must be returned as soon as possible */
    /* don't use any endless or time consuming functions           */
    /* e.g. exit() would be fatal                                  */
    /* defer all time consuming functionality to other threads     */
    /***************************************************************/
    printf("callback_for_ds_write_conf\n");
    printf("this callback must not occur in this sample application\n");
}

/*************************************************************/
/* Get a Character from console                              */
/*************************************************************/
int getCharWithTimeout()
{
    int key = 0;
#ifndef WIN32
    struct pollfd pollfd[1];
    static int init = 0;
    static struct termios termiosOld;
    static struct termios termios;

    if(!init) {
        tcgetattr(fileno(stdin), &termios);
        memcpy(&termiosOld, &termios, sizeof (termios));
        cfmakeraw(&termios);
        termios.c_lflag &= ~ICANON;
        termios.c_lflag &= ~ECHO;
        termios.c_lflag &= ~ISIG;
        termios.c_oflag |= ONLCR | OPOST;
        termios.c_cc[VMIN] = 0;
        termios.c_cc[VTIME] = 0;
        ++init;
    }

    /* put terminal into raw mode */
    tcsetattr(fileno(stdin), TCSANOW, &termios);
    pollfd->fd = fileno(stdin);
    pollfd->events = POLLIN;
    pollfd->revents = 0;
    poll(pollfd, 1, 100);
    if(pollfd->revents & POLLIN)
        key = getchar();

    tcsetattr(fileno(stdin), TCSANOW, &termiosOld);
#else
    Sleep(100);
    if(_kbhit()) {
        key = _getch();
    }
#endif

    return key;
}

/*-------------------------------------------------------------*/
/* this function will be called from IO-BASE to signal a change*/
/* in the opreation mode                                       */
/*                                                             */
/* parameters                                                  */
/*    pCbfPrm         : Callback information                   */
/*-------------------------------------------------------------*/
void callback_for_mode_change_indication(PNIO_CBE_PRM * pCbfPrm)
{
    /**************************************************************/
    /* Attention :                                                */
    /* this is a callback and must be returned as soon as possible */
    /* don't use any endless or time consuming functions          */
    /* e.g. exit() would be fatal                                 */
    /* defer all time consuming functionality to other threads    */
    /**************************************************************/

    /* Check if correct callback type */
    if(pCbfPrm->CbeType == PNIO_CBE_MODE_IND) {
        /* Callback has correct type so check mode change */
        /* and set global variable                        */
        switch (pCbfPrm->ModeInd.Mode) {
            case PNIO_MODE_OFFLINE:
                g_currentMode = PNIO_MODE_OFFLINE;
                break;
            case PNIO_MODE_CLEAR:
                g_currentMode = PNIO_MODE_CLEAR;
                break;
            case PNIO_MODE_OPERATE:
                g_currentMode = PNIO_MODE_OPERATE;
                break;
            default:
                printf("callback_for_mode_change_indication called with wrong mode\n");
                break;
        }
    }
    printf("callback_for_mode_change_indication was called (new mode is %d)\n", g_currentMode);
}

/*-------------------------------------------------------------*/
/* this function will be called from IO-BASE to signal that    */
/* a alarm has been received                                   */
/*                                                             */
/* parameters                                                  */
/*    pCbfPrm         : Callback information                   */
/*-------------------------------------------------------------*/
void callback_for_alarm_indication(PNIO_CBE_PRM * pCbfPrm)
{
    /**************************************************************/
    /* Attention :                                                */
    /* this is a callback and must be returned as soon as possible */
    /* don't use any endless or time consuming functions          */
    /* e.g. exit() would be fatal                                 */
    /* defer all time consuming functionality to other threads    */
    /**************************************************************/

    /* Check if correct callback type */
    if(pCbfPrm->CbeType == PNIO_CBE_ALARM_IND) {
        switch (pCbfPrm->AlarmInd.pAlarmData->AlarmType) {
            case PNIO_ALARM_DIAGNOSTIC:
                printf("PNIO_ALARM_DIAGNOSTIC\n");
                break;

            case PNIO_ALARM_PROCESS:
                printf("PNIO_ALARM_PROCESS\n");
                break;

            case PNIO_ALARM_PULL:
                printf("PNIO_ALARM_PULL\n");
                break;

            case PNIO_ALARM_PLUG:
                printf("PNIO_ALARM_PLUG\n");
                break;

            case PNIO_ALARM_STATUS:
                printf("PNIO_ALARM_STATUS\n");
                break;

            case PNIO_ALARM_UPDATE:
                printf("PNIO_ALARM_UPDATE\n");
                break;

            case PNIO_ALARM_REDUNDANCY:
                printf("PNIO_ALARM_REDUNDACY\n");
                break;

            case PNIO_ALARM_CONTROLLED_BY_SUPERVISOR:
                printf("PNIO_ALARM_CONTROLLED_BY_SUPERVISOR\n");
                break;

            case PNIO_ALARM_RELEASED_BY_SUPERVISOR:
                printf("PNIO_ALARM_RELEASED_BY_SUPERVISOR\n");
                break;

            case PNIO_ALARM_PLUG_WRONG:
                printf("PNIO_ALARM_PLUG_WRONG\n");
                break;

            case PNIO_ALARM_RETURN_OF_SUBMODULE:
                printf("PNIO_ALARM_RETURN_OF_SUBMODULE\n");
                break;

            case PNIO_ALARM_DEV_FAILURE:
                printf("PNIO_ALARM_DEV_FAILURE\n");
                break;

            case PNIO_ALARM_DEV_RETURN:
                printf("PNIO_ALARM_DEV_RETURN\n");
                break;
            default:
                printf("callback_for_alarm_indication called with unknown type\n");
                break;
        }
    }
}
