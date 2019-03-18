TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC += aiSecond

DBD += aiSecond.dbd
aiSecond_SRCS += devAiSecond.c
aiSecond_SRCS += drvAiSecond.c
pniousr_DIR = /usr/lib64/
aiSecond_LIBS += pniousr
aiSecond_LIBS += $(EPICS_BASE_IOC_LIBS)


#=============================
# Build the IOC application

PROD_IOC = Clock1
# Clock1.dbd will be created and installed
DBD += Clock1.dbd

# Clock1.dbd will be made up from these files:
Clock1_DBD += base.dbd

# Include dbd files from all support applications:
Clock1_DBD += aiSecond.dbd

# Add all the support libraries needed by this IOC
Clock1_LIBS += aiSecond

# Clock1_registerRecordDeviceDriver.cpp derives from Clock1.dbd
 Clock1_SRCS += Clock1_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
Clock1_SRCS_DEFAULT += Clock1Main.cpp
Clock1_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#Clock1_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
Clock1_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE