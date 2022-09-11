import numpy as np
import matplotlib.pyplot as plt
import time

class BMData:
    """BMData - the batmon data class for python
    
    bmd = BMData()
        OR
    bmd = BMData('/path/to/data')
"""
    
    def __init__(self, filename='/var/batmon/data'):
        self.filename = filename
        self.start = None
        self.type = []
        self.state = []
        self.time = []
        self.I = []
        self.Istd = []
        self.Imax = []
        self.Imin = []
        self.Vt = []
        self.Voc = []
        self.Q = []
        self.soc = []
        self.T = []
        with open(filename,'r') as ff:
            for thisline in ff:
                self.time.append(int(thisline[1:12]))
                self.type.append(thisline[13])
                self.state.append(thisline[15])
                self.soc.append(float(thisline[16:24]))
                self.I.append(float(thisline[24:32]))
                self.Istd.append(float(thisline[32:40]))
                self.Imin.append(float(thisline[40:48]))
                self.Imax.append(float(thisline[48:56]))
                self.Vt.append(float(thisline[56:64]))
                self.Voc.append(float(thisline[64:72]))
                self.Q.append(float(thisline[72:86]))
                self.T.append(float(thisline[86:92]))

        for index in ['type','state','time','I','Istd','Imax','Imin','Vt','Voc','Q','soc','T']:
            self.__dict__[index] = np.asarray(self.__dict__[index])
        
        
