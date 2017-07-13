CPP=g++
CPP_FLAG=-fPIC -c -O2

INCLUDE=-I ../include

LIBS=../libs/libDFITCMdApiXQN.so

DFITCMDAPI_TRAGET=xqnSocket2ApiDemo

DFITCMdAPI_SCOURE=xqnSocket2ApiDemo.cpp

DFITCMdAPI_RELY=xqnSocket2ApiDemo.o


$(DFITCMdAPI_RELY):$(DFITCMdAPI_SCOURE)
	rm -rf *.o
	rm -rf $(DFITCMDAPI_TRAGET)
	$(CPP) $(INCLUDE) $(LIBS) $(DFITCMdAPI_SCOURE) -lrt -pthread -o $(DFITCMDAPI_TRAGET)
	rm -rf *.o

clean:
	rm -rf *.o
	rm -rf $(DFITCMDAPI_TRAGET)
