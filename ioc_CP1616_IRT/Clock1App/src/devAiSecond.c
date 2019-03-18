#include "drvAiSecond.h"



/*******************************ai*********************************/
  static long cp1616InitRecordAI();
  static long init_ai();
  static long cp1616ReadAI();

  struct devsup {
    long    number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io
  } cp1616Ai = {
    5,
    NULL,
    init_ai,
    cp1616InitRecordAI,
    NULL,
    cp1616ReadAI
  };
   
  epicsExportAddress(dset,cp1616Ai);
  
  static long init_ai(int after)
  {
    return(0);
  }
  
  static long cp1616InitRecordAI(struct aiRecord *pai)
  {
    return(0);
  }
  

  static long cp1616ReadAI(struct aiRecord *pai)
  {  
    pai->udf = FALSE;
    pai->rval = CP_IOC_Input();
    return(0); 
  }
/*******************************ai*********************************/




/*******************************ao*********************************/
  
  static long cp1616InitRecordAo(aoRecord *);
  static long cp1616WriteAo(aoRecord *);
  
  struct devsup cp1616Ao =
  {
      5,
      NULL,
      NULL,
      cp1616InitRecordAo,
      NULL,
      cp1616WriteAo
  };
  
  epicsExportAddress(dset, cp1616Ao);


  static long cp1616InitRecordAo(aoRecord *record)
  {
    return 0;
  }
  
  static long cp1616WriteAo(aoRecord *pao)
  {  
    int val = pao -> rval;
    CP_IOC_Output(val);
    return 0;
  }
/*******************************ao*********************************/
