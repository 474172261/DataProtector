#pragma once

#include "dataprotectorUser.h"
int InitQueue();
void UninitQueue();
int Push(PIDPARAMETER * pData);
PIDPARAMETER* Pop();