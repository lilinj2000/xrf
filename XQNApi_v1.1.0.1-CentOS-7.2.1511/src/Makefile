CPP=g++
CPP_FLAG=-fPIC -c -O2

INCLUDE=-I../include

DFITCMDAPI_TRAGET=../libs/libDFITCMdApiXQN.so

DFITCMdAPI_SCOURE=DFITCMdApiXQN.cpp

DFITCMdAPI_RELY=DFITCMdApiXQN.o


$(DFITCMdAPI_RELY):$(DFITCMdAPI_SCOURE)
	rm -rf *.o
	rm -rf $(DFITCMDAPI_TRAGET)
	$(CPP) $(SHARE) $(INCLUDE) $(CPP_FLAG) $(DFITCMdAPI_SCOURE) -shared -o $(DFITCMDAPI_TRAGET)
	rm -rf *.o

clean:
	rm -rf *.o
	rm -rf $(DFITCMDAPI_TRAGET)
