import ctypes
import threading
import time


gCountinue = True

class PATHARRAY(ctypes.Structure):
    _fields_ = [
        ("orgPath",ctypes.c_wchar * 260),
        ("bakPath",ctypes.c_wchar * 260)
    ]

class PIDINFO(ctypes.Structure):
    _fields_ = [
        ("pid",ctypes.c_uint32),
        ("firstTime",ctypes.c_ulonglong),
        ("imgPath",ctypes.c_wchar * 260),
        ('path', PATHARRAY * 5),
        ('pathCount', ctypes.c_uint32)
    ]

def DPNotifyCallBack(pinfo):
    info = pinfo[0]
    print(info.pid)
    print(info.imgPath)
    print(info.path[0].orgPath)
    print(info.path[1].orgPath)
    print(info.path[2].orgPath)
    print(info.path[3].orgPath)
    print(info.path[4].orgPath)
    gCountinue = False
    return 0


prototype = ctypes.WINFUNCTYPE(ctypes.c_uint32, ctypes.POINTER(PIDINFO))
try:
    dp = ctypes.WinDLL("D:\\work\\pyloader\\dataprotector\\user\\dlldebug\\dataprotectoruser.dll")
except:
    print("Wrong path")
    exit(0)

callback = prototype(DPNotifyCallBack)
StartDP = dp.StartDataProtector
StartDP(callback)